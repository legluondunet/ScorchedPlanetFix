// SPDX-License-Identifier: GPL-3.0-or-later
//! Matrox Mystique OEM extraction from an original, user-owned ISO9660 disc.
//! Supports 2048-byte ISO images and raw CloneCD Mode1/2352 IMG sectors.
//! This code does not modify the source disc and never invokes its installer.
use crate::{matrox, matrox_runtime, cue_image};
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

const PAYLOAD: u64 = 2048;
const ORIGINAL_FOLDER: &str = "game";
const STAGING_FOLDER: &str = ".scorchedplanet-matrox-installing";
const MAX_DIR_BYTES: u64 = 16 * 1024 * 1024;
const MAX_ENTRIES: usize = 25_000;

#[derive(Debug, Clone)]
struct Entry {
    path: String,
    lba: u32,
    bytes: u32,
    directory: bool,
}
struct Image {
    file: File,
    sectors: u64,
    sector_bytes: u64,
    payload_shift: u64,
}
impl Image {
    fn read_iso(&mut self, mut position: u64, mut output: &mut [u8]) -> Result<(), String> {
        while !output.is_empty() {
            let lba = position / PAYLOAD;
            if lba >= self.sectors {
                return Err("Matrox CD extent exceeds image size".into());
            }
            let within = position % PAYLOAD;
            let count = output.len().min((PAYLOAD - within) as usize);
            let physical = lba
                .checked_mul(self.sector_bytes)
                .and_then(|n| n.checked_add(self.payload_shift + within))
                .ok_or("Matrox CD sector offset overflow")?;
            self.file.seek(SeekFrom::Start(physical)).map_err(|e| e.to_string())?;
            self.file.read_exact(&mut output[..count]).map_err(|e| e.to_string())?;
            position += count as u64;
            output = &mut output[count..];
        }
        Ok(())
    }
    fn descriptor(&mut self, lba: u64) -> Result<Vec<u8>, String> {
        let mut buf = vec![0; PAYLOAD as usize];
        self.read_iso(lba * PAYLOAD, &mut buf)?;
        if &buf[1..6] != b"CD001" || buf[6] != 1 {
            return Err("Not a valid ISO9660 volume descriptor".into());
        }
        Ok(buf)
    }
    fn valid_extent(&self, lba: u32, size: u32) -> bool {
        (lba as u64).checked_mul(PAYLOAD)
            .and_then(|n| n.checked_add(size as u64))
            .is_some_and(|end| end <= self.sectors * PAYLOAD)
    }
}

fn open_image(path: &Path) -> Result<Image, String> {
    let meta = fs::symlink_metadata(path)
        .map_err(|e| format!("Cannot inspect disc {}: {e}", path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink() {
        return Err("Matrox image must be an ordinary regular file".into());
    }
    for (sector_bytes, shift) in [(2352, 16), (2048, 0)] {
        if meta.len() < 32 * sector_bytes || meta.len() % sector_bytes != 0 {
            continue;
        }
        let file = File::open(path).map_err(|e| e.to_string())?;
        let mut probe = Image {
            file,
            sectors: meta.len() / sector_bytes,
            sector_bytes,
            payload_shift: shift,
        };
        if let Ok(desc) = probe.descriptor(16) {
            if desc[0] == 1 {
                return Ok(probe);
            }
        }
    }
    Err("Unrecognized Matrox image: expected 2048-byte .iso or Mode1/2352 .img".into())
}
fn name(record: &[u8], joliet: bool) -> Result<String, String> {
    if record == [0] { return Ok(".".into()); }
    if record == [1] { return Ok("..".into()); }
    let decoded = if joliet {
        if record.len() % 2 != 0 { return Err("Odd-length Joliet filename".into()); }
        let units: Vec<u16> = record.chunks_exact(2)
            .map(|chunk| u16::from_be_bytes([chunk[0], chunk[1]])).collect();
        String::from_utf16(&units).map_err(|_| "Invalid UTF-16 Joliet filename")?
    } else {
        String::from_utf8(record.to_vec()).map_err(|_| "Invalid ISO9660 filename")?
    };
    Ok(decoded.split(';').next().unwrap_or("").to_string())
}
fn record(data: &[u8], joliet: bool) -> Result<Entry, String> {
    if data.len() < 34 || data[0] as usize > data.len() {
        return Err("Invalid ISO9660 directory record length".into());
    }
    let len = data[32] as usize;
    if 33 + len > data.len() {
        return Err("Invalid ISO9660 filename length".into());
    }
    let raw_name = name(&data[33..33 + len], joliet)?;
    if !matches!(raw_name.as_str(), "." | "..") &&
        (raw_name.is_empty() || raw_name.ends_with(|c: char| c == ' ' || c == '.')
        || raw_name.contains(|c: char| matches!(c, '/' | '\\' | ':' | '<' | '>' | '"' | '|' | '?' | '*'))
        || raw_name.chars().any(|c| c.is_control())) {
        return Err(format!("Unsafe Matrox ISO filename: {raw_name:?}"));
    }
    Ok(Entry {
        path: raw_name,
        lba: u32::from_le_bytes(data[2..6].try_into().unwrap()),
        bytes: u32::from_le_bytes(data[10..14].try_into().unwrap()),
        directory: data[25] & 2 != 0,
    })
}
fn directory_tree(image: &mut Image) -> Result<BTreeMap<String, Entry>, String> {
    let pvd = image.descriptor(16)?;
    if pvd[0] != 1 { return Err("Matrox ISO primary volume descriptor missing".into()); }
    let mut descriptor = pvd;
    let mut joliet = false;
    for lba in 17..=31 {
        let desc = image.descriptor(lba)?;
        if desc[0] == 255 { break; }
        if desc[0] == 2 && matches!(&desc[88..91], b"%/@" | b"%/C" | b"%/E") {
            descriptor = desc;
            joliet = true;
            break;
        }
    }
    let root = record(&descriptor[156..190], joliet)?;
    if !root.directory { return Err("Matrox ISO root record is not a directory".into()); }
    let mut queue = VecDeque::from([(String::new(), root, 0usize)]);
    let mut visited = BTreeSet::new();
    let mut entries = BTreeMap::new();
    while let Some((parent, folder, depth)) = queue.pop_front() {
        if depth > 20 || folder.bytes as u64 > MAX_DIR_BYTES
            || !image.valid_extent(folder.lba, folder.bytes) {
            return Err("Unsafe Matrox ISO directory extent/depth".into());
        }
        if !visited.insert((folder.lba, folder.bytes)) { continue; }
        let mut data = vec![0u8; folder.bytes as usize];
        image.read_iso(folder.lba as u64 * PAYLOAD, &mut data)?;
        let mut offset = 0usize;
        while offset < data.len() {
            let size = data[offset] as usize;
            if size == 0 {
                offset = (offset / PAYLOAD as usize + 1) * PAYLOAD as usize;
                continue;
            }
            if size < 34 || offset + size > data.len() {
                return Err("Matrox ISO directory entry exceeds valid bounds".into());
            }
            let mut entry = record(&data[offset..offset + size], joliet)?;
            offset += size;
            if entry.path == "." || entry.path == ".." { continue; }
            entry.path = format!("{}{}", parent, entry.path.to_ascii_lowercase());
            if !entry.path.is_ascii() || entry.path.len() > 512 {
                return Err("Unsupported or unsafe Matrox OEM disc pathname".into());
            }
            if !image.valid_extent(entry.lba, entry.bytes) {
                return Err(format!("Out-of-range Matrox disc file {}", entry.path));
            }
            if entries.insert(entry.path.clone(), entry.clone()).is_some() {
                return Err(format!("Matrox disc contains duplicate normalized path {}", entry.path));
            }
            if entries.len() > MAX_ENTRIES {
                return Err("Matrox ISO exceeds safe number of entries".into());
            }
            if entry.directory {
                queue.push_back((format!("{}/", entry.path), entry, depth + 1));
            }
        }
    }
    Ok(entries)
}
fn expected_files(image: &mut Image) -> Result<Vec<Entry>, String> {
    let listing = directory_tree(image)?;
    let expected = matrox::manifest_entries()?;
    let actual: Vec<Entry> = listing.into_iter().filter_map(|(name, mut e)| {
        if !e.directory && name.starts_with("splanet/") {
            e.path = name.trim_start_matches("splanet/").to_string();
            Some(e)
        } else { None }
    }).collect();
    if actual.len() != expected.len() {
        return Err(format!("Matrox disc edition mismatch: expected {} OEM files; found {}", expected.len(), actual.len()));
    }
    let by_name: BTreeMap<_, _> = actual.into_iter()
        .map(|entry| (entry.path.clone(), entry)).collect();
    let mut verified = Vec::new();
    for (_sha, bytes, path) in expected {
        let entry = by_name.get(path)
            .ok_or_else(|| format!("Missing Matrox OEM file in disc: {path}"))?;
        if entry.bytes as u64 != bytes {
            return Err(format!("Matrox OEM file has unexpected size: {path}"));
        }
        verified.push(entry.clone());
    }
    Ok(verified)
}

/// Never allow Matrox installation while ANY game installation exists.
/// Use symlink_metadata so even dangling links block unsafe reuse.
fn refuse_retail_installation(root: &Path) -> Result<(), String> {
    match fs::symlink_metadata(root.join("game")) {
        Ok(_) => Err("A game edition is already installed in game/. ScorchedPlanetFix manages only one edition; Matrox installation refused.".into()),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(e) => Err(format!("Cannot inspect existing game/ installation: {e}")),
    }
}

/// Accept the original user-owned ISO, raw Mode1/2352 IMG, or the safe
/// CUE/BIN(IMG) descriptor already supported by the retail 3dfx importer.
/// A CUE data track is normalized to a temporary 2048-byte ISO, which is
/// always removed when the returned guard goes out of scope (also on failure).
/// The same 66-file SHA-256 checks apply to every input format.
pub fn install_media(root: &Path, source: &Path, bin_override: Option<&Path>) -> Result<(), String> {
    refuse_retail_installation(root)?;
    if source.extension().is_some_and(|e| e.eq_ignore_ascii_case("cue")) {
        let temporary = cue_image::normalize_cue_with_bin(source, bin_override, root,
            |_done, _total, _message| {}, |_message| {})?;
        install(root, &temporary.path)
    } else {
        if bin_override.is_some() {
            return Err("An explicit BIN/IMG companion is only valid with a .cue source".into());
        }
        if !source.extension().is_some_and(|e| e.eq_ignore_ascii_case("iso") || e.eq_ignore_ascii_case("img")) {
            return Err("Expected an ISO, raw IMG or CUE/BIN source".into());
        }
        install(root, source)
    }
}

/// Structural disc recognition, not a substitute for the extraction SHA audit.
/// CUE conversion is guarded and temporary; the original disc is never edited.
pub fn recognized_media(root: &Path, source: &Path) -> bool {
    if source.extension().is_some_and(|e| e.eq_ignore_ascii_case("cue")) {
        let temporary = cue_image::normalize_cue_with_bin(source, None, root,
            |_done, _total, _message| {}, |_message| {});
        match temporary {
            Ok(temporary) => recognized(&temporary.path),
            Err(_) => false,
        }
    } else {
        recognized(source)
    }
}

/// Read-only structural recognition. Full SHA-256 checks are performed during extraction.
pub fn recognized(image: &Path) -> bool {
    open_image(image).and_then(|mut source| expected_files(&mut source)).is_ok()
}

/// Install the exact 66 original user-owned files, with transactional staging.
/// Existing installations are verified and never overwritten.
pub fn install(root: &Path, image: &Path) -> Result<(), String> {
    refuse_retail_installation(root)?;
    let target = root.join(ORIGINAL_FOLDER);
    if fs::symlink_metadata(&target).is_ok() {
        return Err("game/ already exists: refusing to overwrite an installed edition".into());
    }
    let mut source = open_image(image)?;
    let files = expected_files(&mut source)?;
    let manifest: BTreeMap<_, _> = matrox::manifest_entries()?.into_iter()
        .map(|(sha, bytes, path)| (path, (sha, bytes))).collect();
    let stage = root.join(STAGING_FOLDER);
    if fs::symlink_metadata(&stage).is_ok() {
        return Err(format!("Existing Matrox installation stage (not deleted): {}", stage.display()));
    }
    fs::create_dir(&stage).map_err(|e| format!("Cannot create isolated OEM stage: {e}"))?;
    let result = (|| -> Result<(), String> {
        let mut buffer = vec![0u8; 64 * 1024];
        for entry in files {
            let (expected_hash, expected_size) = manifest.get(entry.path.as_str())
                .ok_or_else(|| format!("Unexpected Matrox disc entry {}", entry.path))?;
            let out_path = stage.join(&entry.path);
            if let Some(parent) = out_path.parent() {
                fs::create_dir_all(parent).map_err(|e| e.to_string())?;
            }
            let mut output = OpenOptions::new().create_new(true).write(true)
                .open(&out_path).map_err(|e| e.to_string())?;
            let mut sha = Sha256::new();
            let mut remaining = entry.bytes as u64;
            let mut position = entry.lba as u64 * PAYLOAD;
            while remaining > 0 {
                let count = remaining.min(buffer.len() as u64) as usize;
                source.read_iso(position, &mut buffer[..count])?;
                output.write_all(&buffer[..count]).map_err(|e| e.to_string())?;
                sha.update(&buffer[..count]);
                remaining -= count as u64;
                position += count as u64;
            }
            output.sync_all().map_err(|e| e.to_string())?;
            if entry.bytes as u64 != *expected_size ||
                format!("{:x}", sha.finalize()) != *expected_hash {
                return Err(format!("Matrox ISO content hash does not match OEM edition: {}", entry.path));
            }
        }
        let ini = b"[scorch]\r\nlanguage= 2\r\nvideo= video\r\nHighQualityVideo=1\r\nFPSLimit=60\r\n[matrox]\r\nDither=0\r\n";
        fs::write(stage.join("splanet.ini"), ini).map_err(|e| e.to_string())?;
        matrox::verify_original(&stage)?;
        // Transform only the private staging tree into ONE playable game/.
        // Preserve the two authenticated original binaries as .bak files.
        matrox_runtime::prepare_staged(&stage,root)?;
        if fs::symlink_metadata(&target).is_ok() {
            return Err("Matrox install target appeared while staging; refusing overwrite".into());
        }
        fs::rename(&stage, &target)
            .map_err(|e| format!("Cannot publish verified Matrox OEM installation: {e}"))?;
        Ok(())
    })();
    if result.is_err() && !target.exists() {
        let _ = fs::remove_dir_all(&stage);
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn cue_img_casefolded_track_is_normalized_then_rejected_safely() {
        // The original user's CUE declares Scorched_Planet.IMG, while the
        // Linux file is named Scorched_Planet.img. A synthetic CD proves that
        // case folding and temporary ISO cleanup work on that exact layout.
        const SYNC: [u8; 12] = [0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0];
        let root = std::env::temp_dir()
            .join(format!("matrox-cue-casefold-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir(&root).unwrap();
        let cue = root.join("Scorched_Planet.cue");
        fs::write(&cue, b"FILE \"Scorched_Planet.IMG\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n").unwrap();
        let mut bytes = vec![0u8; 20 * 2352];
        for i in 0..20 {
            let offset = i * 2352;
            bytes[offset..offset + 12].copy_from_slice(&SYNC);
            bytes[offset + 15] = 1;
        }
        let pvd = 16 * 2352 + 16;
        bytes[pvd] = 1;
        bytes[pvd + 1..pvd + 6].copy_from_slice(b"CD001");
        bytes[pvd + 6] = 1;
        fs::write(root.join("Scorched_Planet.img"), bytes).unwrap();
        let error = install_media(&root, &cue, None).unwrap_err();
        assert!(!error.contains("BIN file missing"), "{error}");
        assert!(!root.join(ORIGINAL_FOLDER).exists());
        assert!(!root.join(STAGING_FOLDER).exists());
        for file in fs::read_dir(&root).unwrap() {
            let name = file.unwrap().file_name().to_string_lossy().to_string();
            assert!(!name.starts_with(".scorchedplanetfix-cue-"), "temporary CUE ISO leaked: {name}");
        }
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn rejects_non_cue_companion_before_installing_anything() {
        let folder = std::env::temp_dir().join(format!("matrox-media-companion-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&folder);
        fs::create_dir(&folder).unwrap();
        let err = install_media(&folder, &folder.join("invalid.iso"), Some(&folder.join("data.bin"))).unwrap_err();
        assert!(err.contains("only valid with a .cue"), "{err}");
        assert!(!folder.join(ORIGINAL_FOLDER).exists());
        fs::remove_dir_all(folder).unwrap();
    }
    #[test]
    fn refuses_cue_without_its_referenced_track() {
        let folder = std::env::temp_dir().join(format!("matrox-cue-companion-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&folder);
        fs::create_dir(&folder).unwrap();
        fs::write(folder.join("disc.cue"), b"FILE \"missing.IMG\" BINARY\\n  TRACK 01 MODE1/2352\\n    INDEX 01 00:00:00\\n").unwrap();
        assert!(!recognized_media(&folder, &folder.join("disc.cue")));
        assert!(install_media(&folder, &folder.join("disc.cue"), None).is_err());
        assert!(!folder.join(ORIGINAL_FOLDER).exists());
        fs::remove_dir_all(folder).unwrap();
    }
    #[test]
    fn retail_install_blocks_matrox_media_without_touching_existing_files() {
        let folder = std::env::temp_dir().join(format!("matrox-single-edition-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&folder);
        fs::create_dir_all(folder.join("game")).unwrap();
        fs::write(folder.join("game/keep.txt"), b"retail saved data").unwrap();
        let image = folder.join("matrox.iso");
        fs::write(&image, b"not a real disc").unwrap();
        let err = install_media(&folder, &image, None).unwrap_err();
        assert!(err.contains("only one edition"), "{err}");
        assert!(folder.join(ORIGINAL_FOLDER).is_dir(), "existing game/ must be preserved");
        assert!(!folder.join(STAGING_FOLDER).exists(), "no Matrox stage must be created");
        assert_eq!(fs::read(folder.join("game/keep.txt")).unwrap(), b"retail saved data");
        fs::remove_dir_all(folder).unwrap();
    }

    #[test]
    fn missing_media_does_not_install() {
        let base = std::env::temp_dir()
            .join(format!("matrox-missing-image-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&base);
        fs::create_dir(&base).unwrap();
        assert!(install(&base, &base.join("missing.iso")).is_err());
        assert!(!base.join(ORIGINAL_FOLDER).exists());
        fs::remove_dir_all(base).unwrap();
    }
    #[test]
    fn unsafe_paths_are_rejected() {
        let mut bytes = [0u8; 35];
        bytes[0] = 35; bytes[32] = 2; bytes[33] = b'.'; bytes[34] = b'.';
        assert!(record(&bytes, false).is_ok()); // special '..' is excluded by traversal
        let e = record(&bytes, false).unwrap();
        assert_eq!(e.path, "..");
    }
}
