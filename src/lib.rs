// SPDX-License-Identifier: GPL-3.0-or-later
//! Exact-version, rollback-safe FadeLow patch for Scorched Planet (1996).
//! No game data is bundled, downloaded, or executed by this library.

use std::ffi::OsString;
use std::fs::{self, File, OpenOptions, Permissions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};

pub const ORIGINAL_SHA256: &str =
    "fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a";
/// Previously released four-byte US FadeLow output, accepted only for safe
/// migration when an intact original .bak is present.
pub const FADELOW_SHA256: &str =
    "0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45";
/// Six-byte US FadeLow + CD-media patch, validated by the owner in Wine.
pub const US_FADELOW_NOCD_SHA256: &str =
    "4dc4de1f9034e17b266da7b64a83ba897b4dca9e8cf9a8654f5988d02d1e0613";
/// Legacy EN/ES/IT four-byte FadeLow hash, for migration only.
pub const EN_ES_IT_FADELOW_SHA256: &str =
    "dec3977ff83758f3a65bc5fe96a6bafaa064bbf34ef93ee9f35593fb65e173bc";
/// EN/ES/IT FadeLow + media check patch: six bytes, awaiting gameplay validation.
pub const EN_ES_IT_FADELOW_NOCD_SHA256: &str =
    "66c705a77d290d4bed6abdb4b8b065337aaeb50f9e4480090bbe2dfd5006d35c";
pub const BACKUP_SUFFIX: &str = ".bak";
const LEGACY_BACKUP_SUFFIX: &str = ".ScorchedPlanetFix-original.bak";
pub const TARGET_VA: u32 = 0x0044_95CD;
pub const TARGET_FILE_OFFSET: usize = 0x489CD;
pub const ORIGINAL_BYTES: [u8; 4] = [0x17, 0xB7, 0xD1, 0x38];
pub const FADELOW_BYTES: [u8; 4] = [0xAC, 0xC5, 0x27, 0x37];

/// Only exact owner-supplied and verified game binaries may be patched.
/// The US and FR/DE combinations were confirmed by owner CUE/BIN gameplay tests.
/// The EN/ES/IT six-byte candidate is based on independently verified original
/// CD bytes and matching disassembly; real no-CD gameplay validation is pending.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FadeProfile {
    pub label: &'static str,
    pub original_hash: &'static str,
    pub patched_hash: &'static str,
    pub target_va: u32,
    pub target_offset: usize,
    /// Each edition uses its own verified media-gate instruction location.
    pub media_gate: Option<MediaGate>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MediaGate {
    pub va: u32,
    pub file_offset: usize,
}

const MEDIA_GATE_ORIGINAL: [u8;2] = [0x33,0xc0];
const MEDIA_GATE_REPLACEMENT: [u8;2] = [0xb0,0x01];
pub const US_PROFILE: FadeProfile = FadeProfile {
    label: "US 1.08", original_hash: ORIGINAL_SHA256,
    patched_hash: US_FADELOW_NOCD_SHA256,
    target_va: TARGET_VA, target_offset: TARGET_FILE_OFFSET,
    media_gate: Some(MediaGate {va: 0x0043_581B, file_offset: 0x34C1B}),
};
pub const EN_ES_IT_PROFILE: FadeProfile = FadeProfile {
    label: "EN/ES/IT 1.06",
    original_hash: "9ab6442553d055a14273ee0e3c5a912986662dc0a3cc8512ac716879b5068e5f",
    patched_hash: EN_ES_IT_FADELOW_NOCD_SHA256,
    target_va: 0x0044_93AD, target_offset: 0x487AD,
    media_gate: Some(MediaGate {va: 0x0043_562B, file_offset: 0x34A2B}),
};
pub const FR_DE_PROFILE: FadeProfile = FadeProfile {
    label: "FR/DE 1.06 (FadeLow + CD check fix)",
    original_hash: "eff38cedf2408566060b3c7cf39be59c54d23e67b0a4c7cb0725051aa87e1b72",
    patched_hash: "426882f108348f3e1112597fcbb37af16ef425de5346ecc85425c22c24dc781c",
    target_va: 0x0044_966D, target_offset: 0x48A6D,
    media_gate: Some(MediaGate {va: 0x0043_58BB, file_offset: 0x34CBB}),
};
pub const SUPPORTED_PROFILES: [FadeProfile; 3] = [US_PROFILE, EN_ES_IT_PROFILE, FR_DE_PROFILE];

fn legacy_fadelow_hash(profile: FadeProfile) -> Option<&'static str> {
    if profile.original_hash == ORIGINAL_SHA256 {
        Some(FADELOW_SHA256)
    } else if profile.original_hash == EN_ES_IT_PROFILE.original_hash {
        Some(EN_ES_IT_FADELOW_SHA256)
    } else {
        None
    }
}
pub fn profile_from_hash(hash: &str) -> Option<FadeProfile> {
    SUPPORTED_PROFILES.iter().copied().find(|profile|
        profile.original_hash == hash || profile.patched_hash == hash
        || legacy_fadelow_hash(*profile) == Some(hash))
}

type Result<T> = std::result::Result<T, String>;

/// SHA-256, implemented here to keep release builds dependency-free and
/// reproducible. Known test vectors and independent file hashes are checked.
pub fn sha256_hex(data: &[u8]) -> String {
    const K: [u32; 64] = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    ];
    let mut state: [u32; 8] = [
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    ];
    let mut padded = Vec::with_capacity(data.len() + 72);
    padded.extend_from_slice(data);
    padded.push(0x80);
    while padded.len() % 64 != 56 {
        padded.push(0);
    }
    padded.extend_from_slice(&(data.len() as u64).wrapping_mul(8).to_be_bytes());
    for block in padded.chunks_exact(64) {
        let mut w = [0u32; 64];
        for i in 0..16 {
            w[i] = u32::from_be_bytes(block[i * 4..i * 4 + 4].try_into().unwrap());
        }
        for i in 16..64 {
            let a = w[i - 15];
            let b = w[i - 2];
            let s0 = a.rotate_right(7) ^ a.rotate_right(18) ^ (a >> 3);
            let s1 = b.rotate_right(17) ^ b.rotate_right(19) ^ (b >> 10);
            w[i] = w[i - 16].wrapping_add(s0)
                .wrapping_add(w[i - 7]).wrapping_add(s1);
        }
        let [mut a, mut b, mut c, mut d, mut e, mut f, mut g, mut h] = state;
        for i in 0..64 {
            let s1 = e.rotate_right(6) ^ e.rotate_right(11) ^ e.rotate_right(25);
            let ch = (e & f) ^ ((!e) & g);
            let t1 = h.wrapping_add(s1).wrapping_add(ch)
                .wrapping_add(K[i]).wrapping_add(w[i]);
            let s0 = a.rotate_right(2) ^ a.rotate_right(13) ^ a.rotate_right(22);
            let maj = (a & b) ^ (a & c) ^ (b & c);
            let t2 = s0.wrapping_add(maj);
            h = g; g = f; f = e; e = d.wrapping_add(t1);
            d = c; c = b; b = a; a = t1.wrapping_add(t2);
        }
        for (target, value) in state.iter_mut().zip([a, b, c, d, e, f, g, h]) {
            *target = target.wrapping_add(value);
        }
    }
    let mut out = String::with_capacity(64);
    for word in state {
        use std::fmt::Write as _;
        write!(out, "{word:08x}").expect("write to String cannot fail");
    }
    out
}

fn u16_at(bytes: &[u8], pos: usize) -> Result<u16> {
    let b = bytes.get(pos..pos + 2).ok_or("Truncated PE u16")?;
    Ok(u16::from_le_bytes([b[0], b[1]]))
}
fn u32_at(bytes: &[u8], pos: usize) -> Result<u32> {
    let b = bytes.get(pos..pos + 4).ok_or("Truncated PE u32")?;
    Ok(u32::from_le_bytes(b.try_into().unwrap()))
}

/// Locate the four threshold bytes through the actual PE32 section table.
pub fn pe_target_offset(data: &[u8]) -> Result<usize> {
    pe_target_offset_for(data, US_PROFILE)
}
fn pe_target_offset_for(data: &[u8], profile: FadeProfile) -> Result<usize> {
    pe_site_offset(data, profile.target_va, profile.target_offset, 4)
}
/// Map an instruction's virtual address to a *verified* raw file offset.
fn pe_site_offset(data: &[u8], target_va: u32, expected_offset: usize, width: u32) -> Result<usize> {
    if data.len() < 0x100 || data.get(..2) != Some(b"MZ") {
        return Err("Not a DOS/PE executable (MZ missing)".into());
    }
    let pe = u32_at(data, 0x3c)? as usize;
    if data.get(pe..pe + 4) != Some(&b"PE\0\0"[..]) {
        return Err("Invalid PE signature".into());
    }
    let n = u16_at(data, pe + 6)? as usize;
    let opt_size = u16_at(data, pe + 20)? as usize;
    let opt = pe.checked_add(24).ok_or("PE header overflow")?;
    let sections = opt.checked_add(opt_size).ok_or("PE section overflow")?;
    let end = sections.checked_add(n * 40).ok_or("PE section overflow")?;
    if end > data.len() || opt_size < 32 || u16_at(data, opt)? != 0x10b {
        return Err("Expected valid PE32 section table".into());
    }
    let image_base = u32_at(data, opt + 28)?;
    if image_base != 0x0040_0000 {
        return Err(format!("Unexpected image base 0x{image_base:x}"));
    }
    let rva = target_va.checked_sub(image_base)
        .ok_or("Patch VA precedes PE image base")?;
    for index in 0..n {
        let p = sections + index * 40;
        if data.get(p..p + 5) != Some(&b".text"[..]) {
            continue;
        }
        let virtual_size = u32_at(data, p + 8)?;
        let section_rva = u32_at(data, p + 12)?;
        let raw_size = u32_at(data, p + 16)?;
        let raw_ptr = u32_at(data, p + 20)?;
        let size = virtual_size.min(raw_size);
        if rva >= section_rva
            && rva.checked_add(width).is_some_and(|end_rva| {
                section_rva.checked_add(size).is_some_and(|end_sec| end_rva <= end_sec)
            })
        {
            let offset = (raw_ptr as usize) + (rva - section_rva) as usize;
            if data.get(offset..offset + width as usize).is_some() {
                if offset != expected_offset {
                    return Err("Target file offset differs from verified retail EXE".into());
                }
                return Ok(offset);
            }
        }
    }
    Err("Patch VA not backed by PE .text bytes".into())
}

/// Only patches data matching both the exact source SHA and opcode literal.
/// Also independently verifies full patched-file SHA-256 and length.
fn patch_with_hashes_for(data: &[u8], source: &str, target: &str, profile: FadeProfile) -> Result<Vec<u8>> {
    if sha256_hex(data) != source {
        return Err(format!("Unsupported original executable (SHA-256: {})", sha256_hex(data)));
    }
    let offset = pe_target_offset_for(data, profile)?;
    if data[offset..offset + 4] != ORIGINAL_BYTES {
        return Err("Fade threshold instruction bytes differ from reference".into());
    }
    let mut changed = data.to_vec();
    changed[offset..offset + 4].copy_from_slice(&FADELOW_BYTES);
    if let Some(gate) = profile.media_gate {
        let gate_offset = pe_site_offset(data, gate.va, gate.file_offset, 2)?;
        if data[gate_offset..gate_offset+2] != MEDIA_GATE_ORIGINAL {
            return Err("Media gate instruction bytes differ from reference".into());
        }
        changed[gate_offset..gate_offset+2].copy_from_slice(&MEDIA_GATE_REPLACEMENT);
    }
    if changed.len() != data.len() || sha256_hex(&changed) != target {
        return Err("Patched file SHA-256 mismatch".into());
    }
    Ok(changed)
}
pub fn make_fadelow(data: &[u8]) -> Result<Vec<u8>> {
    let hash = sha256_hex(data);
    let profile = SUPPORTED_PROFILES.iter().copied()
        .find(|candidate| candidate.original_hash == hash)
        .ok_or_else(|| format!("Unsupported original FadeLow edition (SHA-256: {hash})"))?;
    patch_with_hashes_for(data, profile.original_hash, profile.patched_hash, profile)
}

/// Resolve the original executable in an already installed game irrespective
/// of its historical name casing (CD dumps may be uppercase). Reject multiple
/// case-folded matches instead of guessing, and reject symlinks.
pub fn find_installed_exe(directory:&Path)->Result<PathBuf> {
    let metadata=fs::symlink_metadata(directory)
        .map_err(|e|format!("Game directory inaccessible: {e}"))?;
    if !metadata.is_dir() || metadata.file_type().is_symlink(){
        return Err("Invalid splanet directory or symlink".into());
    }
    let mut candidates=Vec::new();
    for entry in fs::read_dir(directory).map_err(|e|e.to_string())? {
        let entry=entry.map_err(|e|e.to_string())?;
        if entry.file_name().to_string_lossy().eq_ignore_ascii_case("spvoodoo.exe") {
            let ty=entry.file_type().map_err(|e|e.to_string())?;
            if !ty.is_file() || ty.is_symlink(){
                return Err("Invalid spvoodoo.exe or symlink".into());
            }
            candidates.push(entry.path());
        }
    }
    match candidates.len() {
        1=>Ok(candidates.remove(0)),
        0=>Err("spvoodoo.exe not found in the splanet directory".into()),
        _=>Err("Multiple spvoodoo.exe files differing by case: ambiguous installation".into()),
    }
}

fn read_regular(path: &Path) -> Result<Vec<u8>> {
    let metadata = fs::symlink_metadata(path)
        .map_err(|e| format!("{}: {e}", path.display()))?;
    if !metadata.is_file() || metadata.file_type().is_symlink() {
        return Err(format!("Not a regular file / symbolic link refused: {}", path.display()));
    }
    fs::read(path).map_err(|e| format!("{}: {e}", path.display()))
}

fn read_backup(path: &Path, expected_hash: Option<&str>) -> Result<Option<Vec<u8>>> {
    match fs::symlink_metadata(path) {
        Ok(meta) => {
            if !meta.is_file() || meta.file_type().is_symlink() {
                return Err(format!("Backup is not a regular file: {}", path.display()));
            }
            let data = read_regular(path)?;
            if Some(sha256_hex(&data).as_str()) != expected_hash {
                return Err(format!("Backup does not match this exact original edition: {}", path.display()));
            }
            Ok(Some(data))
        }
        Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(None),
        Err(e) => Err(format!("Cannot inspect backup {}: {e}", path.display())),
    }
}

fn backup_path_with_suffix(exe: &Path, suffix: &str) -> Result<PathBuf> {
    let mut name: OsString = exe.file_name()
        .ok_or("EXE path has no file name")?.to_os_string();
    name.push(suffix);
    Ok(exe.with_file_name(name))
}
fn backup_path(exe: &Path) -> Result<PathBuf> {
    backup_path_with_suffix(exe, BACKUP_SUFFIX)
}

fn write_new(path: &Path, data: &[u8], permissions: &Permissions, hash: &str) -> Result<()> {
    let mut file = OpenOptions::new().write(true).create_new(true).open(path)
        .map_err(|e| format!("Refusing existing/unwritable destination {}: {e}", path.display()))?;
    let result = (|| {
        file.write_all(data).map_err(|e| e.to_string())?;
        file.sync_all().map_err(|e| e.to_string())?;
        file.set_permissions(permissions.clone()).map_err(|e| e.to_string())?;
        drop(file);
        if sha256_hex(&read_regular(path)?) != hash {
            return Err("New file verification failed".into());
        }
        Ok(())
    })();
    if result.is_err() {
        // We exclusively created this file; it was not present beforehand.
        let _ = fs::remove_file(path);
    }
    result
}

fn replace_atomically(path: &Path, data: &[u8], permissions: &Permissions, hash: &str) -> Result<()> {
    let mut temp = None;
    for nonce in 0..1000 {
        let mut name = path.file_name().ok_or("Missing exe file name")?.to_os_string();
        name.push(format!(".scorchedplanetfix-{}-{nonce}.tmp", std::process::id()));
        let candidate = path.with_file_name(name);
        match OpenOptions::new().write(true).create_new(true).open(&candidate) {
            Ok(f) => { temp = Some((candidate, f)); break; }
            Err(e) if e.kind() == io::ErrorKind::AlreadyExists => continue,
            Err(e) => return Err(format!("Cannot create temporary file: {e}")),
        }
    }
    let (tmp, mut file) = temp.ok_or("Cannot find free temporary name")?;
    let result = (|| {
        file.write_all(data).map_err(|e| e.to_string())?;
        file.sync_all().map_err(|e| e.to_string())?;
        file.set_permissions(permissions.clone()).map_err(|e| e.to_string())?;
        drop(file);
        if sha256_hex(&read_regular(&tmp)?) != hash {
            return Err("Temporary file hash mismatch".into());
        }
        // Replace on same filesystem. On Windows this fails safely if the
        // executable is running/locked: the original backup stays intact.
        fs::rename(&tmp, path).map_err(|e| format!("Cannot replace executable (close game first): {e}"))?;
        if sha256_hex(&read_regular(path)?) != hash {
            return Err("Post-install hash verification failed; use verified backup".into());
        }
        Ok(())
    })();
    if result.is_err() {
        let _ = fs::remove_file(&tmp);
    }
    result
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Action { Status, Copy, Apply, Restore }

pub fn execute(action: Action, exe: &Path, output: Option<&Path>, confirmed: bool) -> Result<String> {
    let source = read_regular(exe)?;
    let hash = sha256_hex(&source);
    let profile = profile_from_hash(&hash);
    let backup = backup_path(exe)?;
    // Even unknown EXEs must not be allowed to use an unrelated existing backup.
    let expected = profile.map(|p| p.original_hash);
    let mut backed = read_backup(&backup, expected)?;
    let perms = fs::symlink_metadata(exe).map_err(|e| e.to_string())?.permissions();
    // Older builds stored a longer filename. On apply/restore, migrate
    // verified original bytes to the requested .bak filename without
    // deleting the old backup. Status/copy remain read-only.
    if backed.is_none() && matches!(action,Action::Apply|Action::Restore) {
        let previous=backup_path_with_suffix(exe,LEGACY_BACKUP_SUFFIX)?;
        if let Some(original)=read_backup(&previous,expected)? {
            write_new(&backup,&original,&perms,expected.ok_or("Unknown backup edition")?)?;
            backed=Some(original);
        }
    }

    match action {
        Action::Status => {
            let state = match profile {
                Some(p) if hash == p.original_hash => format!("ORIGINAL {}", p.label),
                Some(p) if hash == p.patched_hash => format!("PATCHED {}", p.label),
                Some(p) if legacy_fadelow_hash(p)==Some(hash.as_str()) =>
                    format!("{} LEGACY FADELOW (CD check unpatched)",p.label),
                _ => "UNKNOWN/UNSUPPORTED".into(),
            };
            Ok(format!("Executable: {state}\nSHA-256: {hash}\nBackup: {} ({})",
                if backed.is_some() {"VERIFIED ORIGINAL"} else {"ABSENT"},
                backup.display()))
        }
        Action::Copy => {
            let destination = output.ok_or("copy requires --output FILE")?;
            let p = profile.ok_or("Unsupported FadeLow edition")?;
            if hash != p.original_hash { return Err("Expected original unpatched EXE".into()); }
            let patch = patch_with_hashes_for(&source, p.original_hash, p.patched_hash, p)?;
            write_new(destination, &patch, &perms, p.patched_hash)?;
            Ok(format!("FadeLow {} copy created: {}\nSHA-256: {}\nOriginal unchanged.",
                p.label, destination.display(), p.patched_hash))
        }
        Action::Apply => {
            if !confirmed { return Err("apply requires explicit --yes".into()); }
            let p = profile.ok_or("Unsupported FadeLow edition")?;
            if hash == p.patched_hash {
                if backed.is_none() { return Err("Already patched but verified original backup is missing".into()); }
                return Ok(format!("FadeLow {} already patched; verified backup exists.",p.label));
            }
            // Upgrade a legacy US or EN/ES/IT FadeLow-only executable without
            // touching its original backup or user saves. Rebuild the entire
            // six-byte patch from the original, never from a guessed byte state.
            let upgrading_legacy=legacy_fadelow_hash(p)==Some(hash.as_str());
            let (original,patch)=if upgrading_legacy {
                let original=backed.as_ref().ok_or(
                    "Cannot upgrade the legacy FadeLow patch: verified original .bak backup is missing")?;
                let patch=patch_with_hashes_for(original,p.original_hash,p.patched_hash,p)?;
                (original.clone(),patch)
            }else{
                let patch=patch_with_hashes_for(&source,p.original_hash,p.patched_hash,p)?;
                (source.clone(),patch)
            };
            if backed.is_none() {
                write_new(&backup, &original, &perms, p.original_hash)?;
            }
            // A stale or modified running game is never silently overwritten.
            if sha256_hex(&read_regular(exe)?) != hash {
                return Err("EXE modified before replacement; refusing".into());
            }
            replace_atomically(exe,&patch,&perms,p.patched_hash)?;
            Ok(format!("Patch for {} {}applied. Verified original backup: {}",
                p.label,if upgrading_legacy {"upgraded and "}else{""},backup.display()))
        }
        Action::Restore => {
            if !confirmed { return Err("restore requires explicit --yes".into()); }
            let p = profile.ok_or("Unsupported FadeLow edition")?;
            let original = backed.ok_or("Missing verified original backup; restore refused")?;
            if hash == p.original_hash {
                return Ok(format!("Original {} already restored. Backup preserved.",p.label));
            }
            if hash != p.patched_hash &&
               legacy_fadelow_hash(p)!=Some(hash.as_str()) {
                return Err("Unsupported installed EXE; refusing to overwrite".into());
            }
            if sha256_hex(&read_regular(exe)?) != hash {
                return Err("EXE modified before restoration; refusing".into());
            }
            replace_atomically(exe, &original, &perms, p.original_hash)?;
            Ok(format!("Original {} restored. Verified backup preserved at {}",p.label,backup.display()))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn synthetic_pe() -> Vec<u8> {
        let mut data = vec![0u8; 0x60000];
        data[0..2].copy_from_slice(b"MZ");
        data[0x3c..0x40].copy_from_slice(&(0x80u32).to_le_bytes());
        data[0x80..0x84].copy_from_slice(b"PE\0\0");
        data[0x86..0x88].copy_from_slice(&(1u16).to_le_bytes());
        data[0x94..0x96].copy_from_slice(&(0xe0u16).to_le_bytes());
        data[0x98..0x9a].copy_from_slice(&(0x10bu16).to_le_bytes());
        data[0x98+28..0x98+32].copy_from_slice(&(0x400000u32).to_le_bytes());
        let section = 0x80 + 24 + 0xe0;
        data[section..section + 8].copy_from_slice(b".text\0\0\0");
        data[section + 8..section + 12].copy_from_slice(&(0x55000u32).to_le_bytes());
        data[section + 12..section + 16].copy_from_slice(&(0x1000u32).to_le_bytes());
        data[section + 16..section + 20].copy_from_slice(&(0x55000u32).to_le_bytes());
        data[section + 20..section + 24].copy_from_slice(&(0x400u32).to_le_bytes());
        data[TARGET_FILE_OFFSET..TARGET_FILE_OFFSET + 4].copy_from_slice(&ORIGINAL_BYTES);
        data
    }
    fn fixture_dir() -> PathBuf {
        let since = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_nanos();
        let path = env::temp_dir().join(format!("scorched-planet-rust-test-{}-{since}", std::process::id()));
        fs::create_dir(&path).unwrap();
        path
    }
    fn patch_fake(data: &[u8]) -> Vec<u8> {
        let mut expected = data.to_vec();
        expected[TARGET_FILE_OFFSET..TARGET_FILE_OFFSET + 4].copy_from_slice(&FADELOW_BYTES);
        patch_with_hashes_for(data, &sha256_hex(data), &sha256_hex(&expected),
            FadeProfile{media_gate:None,..US_PROFILE}).unwrap()
    }

    #[test] fn us_has_exact_six_byte_media_gate_candidate(){
        assert_eq!(US_PROFILE.media_gate,Some(MediaGate{va:0x43581B,file_offset:0x34C1B}));
        assert_eq!(US_PROFILE.patched_hash,US_FADELOW_NOCD_SHA256);
        assert_eq!(profile_from_hash(FADELOW_SHA256),Some(US_PROFILE));
        assert_eq!(profile_from_hash(US_FADELOW_NOCD_SHA256),Some(US_PROFILE));
        let mut raw=synthetic_pe();
        raw[0x34C1B..0x34C1D].copy_from_slice(&MEDIA_GATE_ORIGINAL);
        let mut expected=raw.clone();
        expected[TARGET_FILE_OFFSET..TARGET_FILE_OFFSET+4].copy_from_slice(&FADELOW_BYTES);
        expected[0x34C1B..0x34C1D].copy_from_slice(&MEDIA_GATE_REPLACEMENT);
        let patched=patch_with_hashes_for(&raw,&sha256_hex(&raw),&sha256_hex(&expected),US_PROFILE)
            .expect("US six-byte patch");
        assert_eq!(patched,expected);
        let changed:Vec<_>=raw.iter().zip(&patched).enumerate()
            .filter(|(_, (before,after))|before!=after).map(|(i,_)|i).collect();
        assert_eq!(changed,vec![0x34C1B,0x34C1C,0x489CD,0x489CE,0x489CF,0x489D0]);
        raw[0x34C1B]=0x90;
        assert!(patch_with_hashes_for(&raw,&sha256_hex(&raw),&sha256_hex(&expected),US_PROFILE).is_err());
    }
    #[test] fn legacy_us_fadelow_copy_is_not_accepted_as_final_build(){
        assert_eq!(profile_from_hash(FADELOW_SHA256),Some(US_PROFILE));
        assert_ne!(FADELOW_SHA256,US_PROFILE.patched_hash);
        assert_eq!(US_PROFILE.original_hash,ORIGINAL_SHA256);
    }
    #[test] fn en_es_it_has_exact_six_byte_media_gate_candidate(){
        // Disassembly of original owner-supplied EN/ES/IT PE32 establishes a
        // matching CD search/return routine at VA 0x435570..0x435645.
        assert_eq!(EN_ES_IT_PROFILE.media_gate,
            Some(MediaGate{va:0x43562B,file_offset:0x34A2B}));
        assert_eq!(EN_ES_IT_PROFILE.patched_hash,EN_ES_IT_FADELOW_NOCD_SHA256);
        assert_eq!(profile_from_hash(EN_ES_IT_FADELOW_SHA256),Some(EN_ES_IT_PROFILE));
        assert_eq!(profile_from_hash(EN_ES_IT_FADELOW_NOCD_SHA256),Some(EN_ES_IT_PROFILE));
        let mut raw=synthetic_pe();
        raw[EN_ES_IT_PROFILE.target_offset..EN_ES_IT_PROFILE.target_offset+4]
            .copy_from_slice(&ORIGINAL_BYTES);
        raw[0x34A2B..0x34A2D].copy_from_slice(&MEDIA_GATE_ORIGINAL);
        let mut expected=raw.clone();
        expected[EN_ES_IT_PROFILE.target_offset..EN_ES_IT_PROFILE.target_offset+4]
            .copy_from_slice(&FADELOW_BYTES);
        expected[0x34A2B..0x34A2D].copy_from_slice(&MEDIA_GATE_REPLACEMENT);
        let patched=patch_with_hashes_for(&raw,&sha256_hex(&raw),&sha256_hex(&expected),
            EN_ES_IT_PROFILE).expect("EN/ES/IT six-byte patch");
        assert_eq!(patched,expected);
        let changed:Vec<_>=raw.iter().zip(&patched).enumerate()
            .filter(|(_, (before,after))|before!=after).map(|(i,_)|i).collect();
        assert_eq!(changed,vec![0x34A2B,0x34A2C,0x487AD,0x487AE,0x487AF,0x487B0]);
        raw[0x34A2B]=0x90;
        assert!(patch_with_hashes_for(&raw,&sha256_hex(&raw),&sha256_hex(&expected),
            EN_ES_IT_PROFILE).is_err());
    }
    #[test] fn legacy_en_es_it_fadelow_can_only_be_upgraded_from_verified_original(){
        assert_eq!(legacy_fadelow_hash(EN_ES_IT_PROFILE),Some(EN_ES_IT_FADELOW_SHA256));
        assert_eq!(legacy_fadelow_hash(US_PROFILE),Some(FADELOW_SHA256));
        assert_eq!(legacy_fadelow_hash(FR_DE_PROFILE),None);
        assert_ne!(EN_ES_IT_FADELOW_SHA256,EN_ES_IT_PROFILE.patched_hash);
        assert_eq!(profile_from_hash(EN_ES_IT_FADELOW_SHA256),Some(EN_ES_IT_PROFILE));
        assert!(profile_from_hash("5373c7b24389556b7466932695e7e2d1076531105f9bb879749e783a64898d90").is_none());
    }
    #[test] fn fr_de_profile_requires_combined_fix() {
        assert_eq!(FR_DE_PROFILE.media_gate, Some(MediaGate{va:0x4358BB,file_offset:0x34CBB}));
        assert_eq!(profile_from_hash(FR_DE_PROFILE.original_hash),Some(FR_DE_PROFILE));
        assert_eq!(profile_from_hash(FR_DE_PROFILE.patched_hash),Some(FR_DE_PROFILE));
        // The earlier FR/DE FadeLow-only candidate is NOT eligible.
        assert!(profile_from_hash("5373c7b24389556b7466932695e7e2d1076531105f9bb879749e783a64898d90").is_none());
    }
    #[test] fn fr_de_synthetic_six_byte_patch_and_gate_rejection() {
        let mut raw=synthetic_pe();
        // Synthetic fixture maps section virtual 0x1000 to raw offset 0x400.
        // Extend its original .text window to cover both mapped patch sites.
        // Original verified file offsets map VA -> file offset by (VA - 0xC00).
        raw[FR_DE_PROFILE.target_offset..FR_DE_PROFILE.target_offset+4].copy_from_slice(&ORIGINAL_BYTES);
        raw[0x34CBB..0x34CBD].copy_from_slice(&MEDIA_GATE_ORIGINAL);
        let mut wanted=raw.clone();
        wanted[FR_DE_PROFILE.target_offset..FR_DE_PROFILE.target_offset+4].copy_from_slice(&FADELOW_BYTES);
        wanted[0x34CBB..0x34CBD].copy_from_slice(&MEDIA_GATE_REPLACEMENT);
        let result=patch_with_hashes_for(&raw,&sha256_hex(&raw),&sha256_hex(&wanted),FR_DE_PROFILE)
            .expect("six-byte FR/DE synthetic patch");
        assert_eq!(result,wanted);
        let changes:Vec<_>=raw.iter().zip(&result).enumerate()
            .filter(|(_, (before,after))|before!=after).map(|(i,_)|i).collect();
        assert_eq!(changes,vec![0x34CBB,0x34CBC,0x48A6D,0x48A6E,0x48A6F,0x48A70]);
        raw[0x34CBB]=0x90;
        assert!(patch_with_hashes_for(&raw,&sha256_hex(&raw),&sha256_hex(&wanted),FR_DE_PROFILE).is_err());
    }
    #[test] fn only_validated_profiles_are_eligible() {
        assert_eq!(profile_from_hash(ORIGINAL_SHA256),Some(US_PROFILE));
        assert_eq!(profile_from_hash(FADELOW_SHA256),Some(US_PROFILE));
        assert_eq!(profile_from_hash(EN_ES_IT_PROFILE.original_hash),Some(EN_ES_IT_PROFILE));
        assert_eq!(profile_from_hash(EN_ES_IT_PROFILE.patched_hash),Some(EN_ES_IT_PROFILE));
        assert_eq!(profile_from_hash(FR_DE_PROFILE.original_hash), Some(FR_DE_PROFILE));
    }
    #[test] fn european_pe_offset_is_edition_specific() {
        let mut data=synthetic_pe();
        data[EN_ES_IT_PROFILE.target_offset..EN_ES_IT_PROFILE.target_offset+4]
            .copy_from_slice(&ORIGINAL_BYTES);
        assert_eq!(pe_target_offset_for(&data, EN_ES_IT_PROFILE).unwrap(),
            EN_ES_IT_PROFILE.target_offset);
        assert_eq!(pe_target_offset_for(&data, US_PROFILE).unwrap(),TARGET_FILE_OFFSET);
        assert!(pe_target_offset_for(&data,FadeProfile{target_offset:0x12345,..EN_ES_IT_PROFILE}).is_err());
    }
    #[test] fn sha256_standard_vectors() {
        assert_eq!(sha256_hex(b""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        assert_eq!(sha256_hex(b"abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        assert_eq!(sha256_hex(&vec![b'a'; 1_000_000]),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }
    #[test] fn maps_verified_pe_offset() {
        let original = synthetic_pe();
        assert_eq!(pe_target_offset(&original).unwrap(), TARGET_FILE_OFFSET);
    }
    #[test] fn only_changes_exact_four_bytes() {
        let original = synthetic_pe();
        let modified = patch_fake(&original);
        assert_eq!(modified.len(), original.len());
        let changes: Vec<_> = original.iter().zip(&modified).enumerate()
            .filter(|(_, (a, b))| a != b).map(|(i, _)| i).collect();
        assert_eq!(changes, vec![TARGET_FILE_OFFSET, TARGET_FILE_OFFSET+1, TARGET_FILE_OFFSET+2, TARGET_FILE_OFFSET+3]);
    }
    #[test] fn rejects_wrong_source_hash_and_original_bytes() {
        let mut raw = synthetic_pe();
        assert!(patch_with_hashes_for(&raw, ORIGINAL_SHA256, FADELOW_SHA256, US_PROFILE).is_err());
        raw[TARGET_FILE_OFFSET] = 0x90;
        let mut desired=raw.clone();
        desired[TARGET_FILE_OFFSET..TARGET_FILE_OFFSET+4].copy_from_slice(&FADELOW_BYTES);
        assert!(patch_with_hashes_for(&raw, &sha256_hex(&raw), &sha256_hex(&desired), US_PROFILE).is_err());
    }
    #[test] fn rejects_non_pe_and_wrong_image_base() {
        let mut raw=synthetic_pe();
        raw[0x80] = 0x00;
        assert!(pe_target_offset(&raw).is_err());
        raw=synthetic_pe();
        raw[0x98+28]=0x12;
        assert!(pe_target_offset(&raw).is_err());
    }
    #[test] fn create_new_never_overwrites() {
        let dir=fixture_dir();
        let path=dir.join("existing.exe");
        fs::write(&path,b"keep").unwrap();
        let perms=fs::metadata(&path).unwrap().permissions();
        assert!(write_new(&path,b"modified",&perms,&sha256_hex(b"modified")).is_err());
        assert_eq!(fs::read(&path).unwrap(),b"keep");
        fs::remove_dir_all(dir).unwrap();
    }
    #[test] fn backup_and_swap_and_restore_synthetic_data() {
        let dir=fixture_dir();
        let exe=dir.join("SPVOODOO.EXE");
        let input=synthetic_pe();
        let fadelow=patch_fake(&input);
        fs::write(&exe,&input).unwrap();
        let perms=fs::metadata(&exe).unwrap().permissions();
        let backup=backup_path(&exe).unwrap();
        write_new(&backup,&input,&perms,&sha256_hex(&input)).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),input);
        replace_atomically(&exe,&fadelow,&perms,&sha256_hex(&fadelow)).unwrap();
        assert_eq!(fs::read(&exe).unwrap(),fadelow);
        replace_atomically(&exe,&input,&perms,&sha256_hex(&input)).unwrap();
        assert_eq!(fs::read(&exe).unwrap(),input);
        assert_eq!(fs::read(&backup).unwrap(),input);
        fs::remove_dir_all(dir).unwrap();
    }
    #[test] fn find_installed_exe_accepts_upper_and_lower_casing(){
        let dir=fixture_dir();
        assert!(find_installed_exe(&dir).is_err());
        let upper=dir.join("SPVOODOO.EXE");
        fs::write(&upper,b"placeholder").unwrap();
        assert_eq!(find_installed_exe(&dir).unwrap(),upper);
        #[cfg(not(windows))]
        {
            let lower=dir.join("spvoodoo.exe");
            fs::write(&lower,b"placeholder").unwrap();
            assert!(find_installed_exe(&dir).unwrap_err().contains("Plusieurs"));
        }
        fs::remove_dir_all(dir).unwrap();
    }
    #[test] fn requested_backup_name_is_bak() {
        assert_eq!(backup_path(Path::new("SPVOODOO.EXE")).unwrap(),
            PathBuf::from("SPVOODOO.EXE.bak"));
        assert_eq!(backup_path_with_suffix(Path::new("SPVOODOO.EXE"),LEGACY_BACKUP_SUFFIX).unwrap(),
            PathBuf::from("SPVOODOO.EXE.ScorchedPlanetFix-original.bak"));
    }
    #[test] fn refusing_unknown_backup() {
        let dir=fixture_dir();
        let exe=dir.join("SPVOODOO.EXE");
        fs::write(&exe,b"anything").unwrap();
        fs::write(backup_path(&exe).unwrap(),b"corrupt").unwrap();
        assert!(read_backup(&backup_path(&exe).unwrap(),None).is_err());
        assert!(execute(Action::Status,&exe,None,false).is_err());
        fs::remove_dir_all(dir).unwrap();
    }
}
