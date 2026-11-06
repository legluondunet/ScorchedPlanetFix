// SPDX-License-Identifier: GPL-3.0-or-later
//! GameFrame Glide 2.11 -> OpenGL experiment.
//! Builds from pinned Apache-2.0 source, embeds the 32-bit DLL into the
//! Windows x64 launcher, and installs it only where glide.dll is absent.
//! Never bundles proprietary nGlide or overwrites the user's own Glide DLL.
use crate::nglide;
use sha2::{Digest, Sha256};
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::Path;
use std::process::Command;

#[cfg(gameframe_embedded)]
pub const GLIDE_DLL: &[u8] = include_bytes!(env!("SCORCHED_GAMEFRAME_DLL_PATH"));
#[cfg(not(gameframe_embedded))]
pub const GLIDE_DLL: &[u8] = &[];

pub const SOURCE_COMMIT: &str = "f195a074a63cc150673c38becdec696163b0b3d1";

fn digest(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}

pub fn embedded_sha256() -> Option<&'static str> {
    option_env!("SCORCHED_GAMEFRAME_SHA256")
}

fn pe32_dll(bytes: &[u8]) -> bool {
    if !(4096..=16 * 1024 * 1024).contains(&bytes.len()) || &bytes[..2] != b"MZ" {
        return false;
    }
    let pos = u32::from_le_bytes(bytes[0x3c..0x40].try_into().unwrap()) as usize;
    if pos.checked_add(26).is_none_or(|end| end > bytes.len())
        || &bytes[pos..pos + 4] != b"PE\0\0" {
        return false;
    }
    let arch = u16::from_le_bytes(bytes[pos+4..pos+6].try_into().unwrap());
    let flags = u16::from_le_bytes(bytes[pos+22..pos+24].try_into().unwrap());
    let optional = u16::from_le_bytes(bytes[pos+24..pos+26].try_into().unwrap());
    arch == 0x014c && flags & 0x2000 != 0 && optional == 0x010b
}

pub fn validate_embedded() -> Result<Option<String>, String> {
    if GLIDE_DLL.is_empty() {
        if embedded_sha256().is_some() {
            return Err("GameFrame SHA-256 set but no DLL is embedded".into());
        }
        return Ok(None);
    }
    if !pe32_dll(GLIDE_DLL) {
        return Err("Embedded GameFrame is not a valid Windows x86 PE32 DLL".into());
    }
    let actual = digest(GLIDE_DLL);
    let expected = embedded_sha256().ok_or("Embedded GameFrame SHA-256 is missing")?;
    if actual != expected {
        return Err(format!("Embedded GameFrame DLL checksum mismatch: {actual}"));
    }
    Ok(Some(actual))
}

fn regular_dll(path: &Path) -> Result<Vec<u8>, String> {
    let metadata = fs::symlink_metadata(path)
        .map_err(|e| format!("Cannot inspect {}: {e}", path.display()))?;
    if metadata.file_type().is_symlink() || !metadata.is_file() {
        return Err(format!("Invalid or symlinked Glide DLL: {}", path.display()));
    }
    let data=fs::read(path).map_err(|e| format!("Cannot read {}: {e}",path.display()))?;
    if !pe32_dll(&data) {
        return Err(format!("Not a Windows 32-bit Glide DLL: {}",path.display()));
    }
    Ok(data)
}

/// Detect only our *exact* bundled GameFrame build. Unknown valid Glide DLLs
/// remain user-owned and use the existing nGlide-compatible launch path.
pub fn is_embedded_gameframe(game: &Path) -> Result<bool, String> {
    let Some(expected) = validate_embedded()? else { return Ok(false); };
    let path=game.join("glide.dll");
    Ok(digest(&regular_dll(&path)?) == expected)
}

/// Safe installer fallback. This can run inside a private game staging folder,
/// or after a complete read-only game-data audit on an existing installation.
/// No pre-existing regular file or symlink is ever replaced.
pub fn ensure_glide(root: &Path, game: &Path, mut log: impl FnMut(&str))
    -> Result<bool, String> {
    let path=game.join("glide.dll");
    match fs::symlink_metadata(&path) {
        Ok(_) => {
            nglide::verify_local_glide(game)?;
            let provider = if is_embedded_gameframe(game)? { "GameFrame OpenGL" } else { "existing user-provided Glide" };
            log(&format!("GLIDE: kept pre-existing 32-bit DLL provider={provider} (not overwritten)"));
            return Ok(true);
        }
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
        Err(e) => return Err(format!("Cannot inspect glide.dll before installation: {e}")),
    }

    let Some(expected)=validate_embedded()? else {
        // The ordinary build is still able to use the user's standalone nGlide
        // installation. The experimental CI always embeds GameFrame.
        return nglide::copy_installed_glide(root, game, log);
    };
    let mut file=OpenOptions::new().write(true).create_new(true).open(&path)
        .map_err(|e|format!("Refusing to overwrite existing Glide DLL: {e}"))?;
    let result=(||{
        file.write_all(GLIDE_DLL).map_err(|e|format!("Cannot write bundled GameFrame: {e}"))?;
        file.flush().map_err(|e|format!("Cannot flush bundled GameFrame: {e}"))?;
        file.sync_all().map_err(|e|format!("Cannot sync bundled GameFrame: {e}"))?;
        drop(file);
        nglide::verify_local_glide(game)?;
        if digest(&regular_dll(&path)?)!=expected {
            return Err("GameFrame DLL hash mismatch after installing it".into());
        }
        Ok(())
    })();
    if let Err(e)=result {
        // Only a newly created file can be removed on error, never a
        // pre-existing user DLL. The original CD and saves stay untouched.
        let _=fs::remove_file(&path);
        return Err(e);
    }
    log(&format!("GAMEFRAME: installed bundled 32-bit glide.dll source={SOURCE_COMMIT} sha256={expected}, backend=OpenGL, provenance=Apache-2.0"));
    Ok(true)
}

/// The existing launcher holds the single game-session log beside its EXE.
/// GameFrame itself appends all its Glide calls, gaps and render issues into
/// that same log via GLIDE_LOG (the upstream logger opens in append mode).
pub fn configure_process(command: &mut Command, game: &Path, show_3dfx_logo: bool,
                         mut log: impl FnMut(&str)) -> Result<(), String> {
    nglide::verify_local_glide(game)?;
    if is_embedded_gameframe(game)? {
        if show_3dfx_logo {
            return Err("--3dfx-logo requires a user-provided nGlide DLL and 3DfxSpl.dll; the bundled GameFrame Glide wrapper does not implement nGlide's splash setting. The existing GameFrame DLL was preserved.".into());
        }
        // GameFrame's neutral 2x scaling retains the original 640x480 4:3
        // framebuffer, as a first compatibility probe (1280x960 output).
        // Native fullscreen / aspect-correct desktop scaling is NOT claimed.
        command.env("GLIDE_SCALE","2");
        command.env_remove("GLIDE_SIZE");
        command.env("GLIDE_LFB_FILTER","nearest");
        if let Some(root)=game.parent() {
            command.env("GLIDE_LOG",root.join("ScorchedPlanetFix.log"));
        }
        let prior=std::env::var("WINEDLLOVERRIDES").unwrap_or_default();
        command.env("WINEDLLOVERRIDES",format!("glide=n;{prior}"));
        log("GAMEFRAME: backend=OpenGL, API=Glide 2.11, 640x480 internal, initial GLIDE_SCALE=2, 4:3, per-game DLL override=glide=n; nGlide settings not applied");
        Ok(())
    }else{
        if show_3dfx_logo {
            nglide::check_splash_plugin(game,|line|log(line))?;
        }
        log("GLIDE: pre-existing external wrapper detected; keeping original nGlide-compatible process configuration");
        nglide::configure_process(command,show_3dfx_logo,|line|log(line));
        Ok(())
    }
}

#[cfg(test)]
mod tests{
    use super::*;

    fn fixture() -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!("scorched-gameframe-{}-{}",
            std::process::id(),
            std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH)
                .unwrap().as_nanos()));
        fs::create_dir(&dir).unwrap();
        dir
    }

    #[test]
    fn bad_pe_or_64_bit_dll_never_accepted(){
        assert!(!pe32_dll(&vec![0u8;4096]));
        let mut fake=vec![0;4096];
        fake[0..2].copy_from_slice(b"MZ");
        fake[0x3c..0x40].copy_from_slice(&0x80u32.to_le_bytes());
        fake[0x80..0x84].copy_from_slice(b"PE\0\0");
        fake[0x84..0x86].copy_from_slice(&0x14cu16.to_le_bytes());
        fake[0x96..0x98].copy_from_slice(&0x2000u16.to_le_bytes());
        fake[0x98..0x9a].copy_from_slice(&0x10bu16.to_le_bytes());
        assert!(pe32_dll(&fake));
        fake[0x98..0x9a].copy_from_slice(&0x20bu16.to_le_bytes());
        assert!(!pe32_dll(&fake));
    }

    #[test]
    fn embedded_sha256_and_pe_are_consistent() {
        assert_eq!(validate_embedded().unwrap().is_some(),!GLIDE_DLL.is_empty());
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn installs_bundled_gameframe_once_and_keeps_exact_bytes(){
        let root=fixture();
        let expected=validate_embedded().unwrap().unwrap();
        assert!(ensure_glide(&root,&root,|_|{}).unwrap());
        let dll=root.join("glide.dll");
        assert_eq!(digest(&fs::read(&dll).unwrap()),expected);
        assert!(is_embedded_gameframe(&root).unwrap());
        assert!(ensure_glide(&root,&root,|_|{}).unwrap());
        assert_eq!(digest(&fs::read(&dll).unwrap()),expected);
        let mut cmd=Command::new("test.exe");
        configure_process(&mut cmd,&root,false,|_|{}).unwrap();
        assert!(cmd.get_envs().any(|(key,val)|key=="GLIDE_LOG" && val.is_some()));
        assert!(configure_process(&mut cmd,&root,true,|_|{}).is_err());
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn refuses_to_replace_existing_invalid_user_glide(){
        let root=fixture();
        let file=root.join("glide.dll");
        fs::write(&file,b"not a DLL").unwrap();
        assert!(ensure_glide(&root,&root,|_|{}).is_err());
        assert_eq!(fs::read(&file).unwrap(),b"not a DLL");
        fs::remove_dir_all(&root).unwrap();
    }
}
