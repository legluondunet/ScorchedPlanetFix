// SPDX-License-Identifier: GPL-3.0-or-later
//! User-owned ScorchedPlanetFix.ini beside the launcher, shared by all editions.
//! Native game/splanet.ini is separate and never used for fix-specific options.
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::Path;

const MAX_INI_SIZE: usize = 256 * 1024;
pub const DEFAULT_FPS: u32 = 60;
pub const MAX_FPS: u32 = 240;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GameSettings {
    /// 0 disables frame pacing; 1..=240 caps the maximum frame rate.
    pub fps_limit: u32,
    pub dither: bool,
}
impl Default for GameSettings {
    fn default() -> Self { Self { fps_limit: DEFAULT_FPS, dither: false } }
}

fn read_bytes(path: &Path) -> Result<Option<Vec<u8>>, String> {
    match fs::symlink_metadata(path) {
        Ok(meta) => {
            if !meta.is_file() || meta.file_type().is_symlink() {
                return Err(format!("Configuration must be a regular INI file: {}", path.display()));
            }
            if meta.len() > MAX_INI_SIZE as u64 {
                return Err("ScorchedPlanetFix.ini exceeds the 256 KiB limit".into());
            }
            let bytes = fs::read(path).map_err(|e| format!("Cannot read {}: {e}", path.display()))?;
            if bytes.len() > MAX_INI_SIZE {
                return Err("ScorchedPlanetFix.ini exceeds the 256 KiB limit".into());
            }
            Ok(Some(bytes))
        }
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(e) => Err(format!("Cannot inspect {}: {e}", path.display())),
    }
}

#[derive(Default)]
struct Presence {
    scorch: bool,
    fps: bool,
    matrox: bool,
    dither: bool,
}

fn parse(bytes: &[u8]) -> Result<(GameSettings, Presence), String> {
    let mut settings = GameSettings::default();
    let mut present = Presence::default();
    let mut section = 0u8;
    for line in bytes.split(|c| *c == b'\n') {
        let line = line.trim_ascii();
        if line.is_empty() || line.starts_with(b";") || line.starts_with(b"#") { continue; }
        if line.starts_with(b"[") && line.ends_with(b"]") {
            section = if line.eq_ignore_ascii_case(b"[scorch]") {
                present.scorch = true;
                1
            } else if line.eq_ignore_ascii_case(b"[matrox]") {
                present.matrox = true;
                2
            } else { 0 };
            continue;
        }
        let Some(eq) = line.iter().position(|b| *b == b'=') else { continue };
        let key = line[..eq].trim_ascii();
        let value = line[eq + 1..].trim_ascii();
        if section == 1 && key.eq_ignore_ascii_case(b"FPSLimit") {
            if present.fps { return Err("INI: duplicate [scorch] FPSLimit".into()); }
            present.fps = true;
            if value.is_empty() || !value.iter().all(u8::is_ascii_digit) {
                return Err("INI: FPSLimit must be a decimal number (0..240)".into());
            }
            let text = std::str::from_utf8(value).map_err(|_| "INI: invalid FPSLimit")?;
            let fps: u32 = text.parse().map_err(|_| "INI: invalid FPSLimit")?;
            if fps > MAX_FPS {
                return Err(format!("INI: FPSLimit must be between 0 and {MAX_FPS}"));
            }
            settings.fps_limit = fps;
        } else if section == 2 && key.eq_ignore_ascii_case(b"Dither") {
            if present.dither { return Err("INI: duplicate [matrox] Dither".into()); }
            present.dither = true;
            settings.dither = match value {
                b"0" => false,
                b"1" => true,
                _ => return Err("INI: [matrox] Dither must be 0 or 1".into()),
            };
        }
    }
    Ok((settings, present))
}

/// Missing fixer INI defaults to 60 FPS and Matrox dither OFF. Normal
/// launcher startup creates it when needed; --verify and unchecked do not.
pub fn read_settings(path: &Path) -> Result<GameSettings, String> {
    match read_bytes(path)? {
        Some(bytes) => Ok(parse(&bytes)?.0),
        None => Ok(GameSettings::default()),
    }
}

fn append_line(out: &mut Vec<u8>, line: &[u8]) {
    if !out.is_empty() && !out.ends_with(b"\n") { out.extend_from_slice(b"\r\n"); }
    out.extend_from_slice(line);
    out.extend_from_slice(b"\r\n");
}

/// Add missing opt-in settings while retaining every existing user value,
/// comment, section and legacy non-UTF8 byte. Never overwrite a user choice.
/// Called only during installation/preparation or verified normal launch;
/// --verify and --launch-no-verify never call it.
pub fn ensure_defaults(path: &Path, matrox: bool) -> Result<(), String> {
    let existing = read_bytes(path)?;
    let original = existing.as_deref().unwrap_or(b"");
    let (_, present) = parse(original)?;
    let add_fps = !present.fps;
    let add_dither = matrox && !present.dither;
    if !add_fps && !add_dither { return Ok(()); }

    let mut out = Vec::with_capacity(original.len() + 96);
    let mut current = 0u8;
    let mut fps_added = false;
    let mut dither_added = false;
    for line in original.split_inclusive(|b| *b == b'\n') {
        let trimmed = line.trim_ascii();
        if trimmed.starts_with(b"[") && trimmed.ends_with(b"]") {
            if current == 1 && add_fps && !fps_added {
                append_line(&mut out, b"FPSLimit=60");
                fps_added = true;
            }
            if current == 2 && add_dither && !dither_added {
                append_line(&mut out, b"Dither=0");
                dither_added = true;
            }
            current = if trimmed.eq_ignore_ascii_case(b"[scorch]") { 1 }
                else if trimmed.eq_ignore_ascii_case(b"[matrox]") { 2 }
                else { 0 };
        }
        out.extend_from_slice(line);
    }
    if current == 1 && add_fps && !fps_added {
        append_line(&mut out, b"FPSLimit=60");
        fps_added = true;
    }
    if current == 2 && add_dither && !dither_added {
        append_line(&mut out, b"Dither=0");
        dither_added = true;
    }
    if !present.scorch {
        append_line(&mut out, b"[scorch]");
    }
    if add_fps && !fps_added {
        append_line(&mut out, b"FPSLimit=60");
    }
    if matrox && !present.matrox {
        append_line(&mut out, b"[matrox]");
    }
    if add_dither && !dither_added {
        append_line(&mut out, b"Dither=0");
    }
    if out.len() > MAX_INI_SIZE { return Err("Adding settings would exceed INI size limit".into()); }
    if parse(&out)?.0 != parse(original)?.0 {
        return Err("INI migration changed an existing value; refusing".into());
    }
    let temporary = path.with_file_name(format!(
        ".scorchedplanetfix-settings-{}-{}.tmp", std::process::id(),
        std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH)
            .map_err(|e| e.to_string())?.as_nanos()));
    let result = (|| -> Result<(), String> {
        let mut file = OpenOptions::new().write(true).create_new(true)
            .open(&temporary).map_err(|e| format!("Cannot stage INI defaults: {e}"))?;
        file.write_all(&out).map_err(|e| e.to_string())?;
        file.sync_all().map_err(|e| e.to_string())?;
        if existing.is_some() {
            file.set_permissions(fs::metadata(path).map_err(|e| e.to_string())?.permissions())
                .map_err(|e| e.to_string())?;
        }
        drop(file);
        fs::rename(&temporary, path).map_err(|e| format!("Cannot update game INI: {e}"))
    })();
    if result.is_err() { let _ = fs::remove_file(&temporary); }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fixture() -> std::path::PathBuf {
        let path = std::env::temp_dir().join(format!(
            "scorched-ini-settings-{}-{}", std::process::id(),
            std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH)
                .unwrap().as_nanos()));
        fs::create_dir_all(&path).unwrap();
        path
    }

    #[test]
    fn fps_and_matrox_dither_are_independent_and_case_insensitive() {
        let config = b"[SCORCH]\nvideo= video\nfpslimit = 120\n[Matrox]\nDITHER=1\n";
        assert_eq!(parse(config).unwrap().0, GameSettings { fps_limit: 120, dither: true });
        assert_eq!(parse(b"[scorch]\nFPSLimit=0\n").unwrap().0.fps_limit, 0);
        assert_eq!(parse(b"[scorch]\n").unwrap().0, GameSettings::default());
        for bad in [b"FPSLimit=-1".as_slice(), b"FPSLimit=241", b"FPSLimit=abc"] {
            let mut bytes=b"[scorch]\n".to_vec();
            bytes.extend_from_slice(bad);
            assert!(parse(&bytes).is_err());
        }
        assert!(parse(b"[scorch]\nFPSLimit=60\nfpslimit=30").is_err());
        assert!(parse(b"[matrox]\nDither=true").is_err());
        assert!(parse(b"[matrox]\nDither=0\nDITHER=1").is_err());
    }

    #[test]
    fn creates_only_missing_settings_and_preserves_users_ini() {
        let dir = fixture();
        let ini = dir.join("ScorchedPlanetFix.ini");
        fs::write(&ini, b"[scorch]\r\nlanguage=2\r\n[Other]\r\nnotes=\xff\r\n[Matrox]\r\nDither=1\r\n").unwrap();
        ensure_defaults(&ini, true).unwrap();
        let bytes = fs::read(&ini).unwrap();
        assert!(bytes.windows(11).any(|w| w == b"FPSLimit=60"));
        assert!(bytes.windows(8).any(|w| w == b"Dither=1"));
        assert!(bytes.windows(7).any(|w| w == b"notes=\xff"));
        assert_eq!(read_settings(&ini).unwrap(), GameSettings { fps_limit: 60, dither: true });
        ensure_defaults(&ini, true).unwrap();
        assert_eq!(fs::read(&ini).unwrap(), bytes);
        fs::write(&ini, b"[scorch]\nFPSLimit=45\n").unwrap();
        ensure_defaults(&ini, false).unwrap();
        assert_eq!(read_settings(&ini).unwrap().fps_limit, 45);
        assert!(!fs::read(&ini).unwrap().windows(8).any(|w| w == b"[matrox]"));
        fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn matrox_section_is_added_and_uncategorized_files_are_refused() {
        let dir=fixture();
        let ini=dir.join("ScorchedPlanetFix.ini");
        fs::write(&ini,b"[scorch]\nvideo= video\n").unwrap();
        ensure_defaults(&ini,true).unwrap();
        let text=fs::read_to_string(&ini).unwrap();
        assert!(text.contains("[matrox]\r\nDither=0\r\n"));
        assert!(text.contains("FPSLimit=60\r\n"));
        fs::remove_file(&ini).unwrap();
        assert_eq!(read_settings(&ini).unwrap(),GameSettings::default());
        ensure_defaults(&ini,true).unwrap();
        let restored=fs::read_to_string(&ini).unwrap();
        assert!(restored.contains("FPSLimit=60"));
        assert!(restored.contains("[matrox]"));
        fs::remove_dir_all(dir).unwrap();
    }
}
