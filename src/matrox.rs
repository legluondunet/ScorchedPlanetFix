// SPDX-License-Identifier: GPL-3.0-or-later
//! Matrox Mystique OEM (1996): strict, read-only verification of user-owned files.
//! This is the first integration stage, NOT a playable MSI95 backend yet.
//! Neither original drivers nor game data are bundled in this repository.

use sha2::{Digest, Sha256};
use std::fs::{self, File};
use std::io::Read;
use std::path::{Component, Path};

const MANIFEST: &str =
    include_str!("../data/cd-inventories/scorched-matrox-mystique-oem.sha256");
const OEM_FILES: usize = 66;
const OEM_TOTAL_BYTES: u64 = 315_947_520;
const ORIGINAL_EXE: &str =
    "1f2684205f4aa349183acfef6eb660e92e6bb24732e5c56278f0df1111108894";
const ORIGINAL_MSI95: &str =
    "71b9921d8225403299fc73932a49ce5d3c1261a3bf7a852a3659d3f81cee3d9e";

#[derive(Debug, PartialEq, Eq)]
pub struct VerifiedOem {
    pub files: usize,
    pub bytes: u64,
}

fn manifest_entries() -> Result<Vec<(&'static str, u64, &'static str)>, String> {
    let mut entries = Vec::new();
    for (line_no, raw) in MANIFEST.lines().enumerate() {
        let raw = raw.trim();
        if raw.is_empty() || raw.starts_with('#') {
            continue;
        }
        let mut fields = raw.split_whitespace();
        let sha = fields.next().ok_or("Missing OEM manifest SHA-256")?;
        let size = fields.next().ok_or("Missing OEM manifest size")?;
        let path = fields.next().ok_or("Missing OEM manifest path")?;
        if fields.next().is_some()
            || sha.len() != 64
            || !sha.bytes().all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase())
            || !path.is_ascii()
            || path.contains('\\')
            || path.starts_with('/')
            || Path::new(path).components().any(|c| !matches!(c, Component::Normal(_)))
        {
            return Err(format!("Invalid Matrox manifest entry at line {}", line_no + 1));
        }
        let size: u64 = size.parse()
            .map_err(|_| format!("Invalid Matrox manifest size at line {}", line_no + 1))?;
        if entries.iter().any(|(_, _, prev)| *prev == path) {
            return Err(format!("Duplicate Matrox manifest path: {path}"));
        }
        entries.push((sha, size, path));
    }
    if entries.len() != OEM_FILES || entries.iter().map(|e| e.1).sum::<u64>() != OEM_TOTAL_BYTES {
        return Err("Incomplete or modified OEM manifest".into());
    }
    if !entries.iter().any(|e| e.2 == "splanetw.exe" && e.0 == ORIGINAL_EXE)
        || !entries.iter().any(|e| e.2 == "msi95.dll" && e.0 == ORIGINAL_MSI95)
    {
        return Err("OEM executable or original driver fingerprint does not match".into());
    }
    Ok(entries)
}

fn regular_not_symlink(path: &Path) -> Result<std::fs::Metadata, String> {
    let meta = fs::symlink_metadata(path)
        .map_err(|e| format!("Cannot inspect {}: {e}", path.display()))?;
    if meta.file_type().is_symlink() || !meta.is_file() {
        return Err(format!("Not a regular file (or symlink): {}", path.display()));
    }
    Ok(meta)
}

/// Read-only SHA-256 audit of the 66 immutable original Matrox OEM files.
/// The user's splanet.ini and future local saves are deliberately excluded.
/// Every intermediate directory is inspected without following symlinks.
pub fn verify_original(folder: &Path) -> Result<VerifiedOem, String> {
    let root_meta = fs::symlink_metadata(folder)
        .map_err(|e| format!("Cannot inspect Matrox installation: {e}"))?;
    if !root_meta.is_dir() || root_meta.file_type().is_symlink() {
        return Err(format!("Matrox game directory is invalid or symlinked: {}", folder.display()));
    }
    let entries = manifest_entries()?;
    let mut checked = 0;
    let mut bytes = 0;
    for (expected_sha, expected_size, rel) in entries {
        let relative = Path::new(rel);
        let mut part = folder.to_path_buf();
        if let Some(parent) = relative.parent() {
            for component in parent.components() {
                part.push(component.as_os_str());
                let meta = fs::symlink_metadata(&part)
                    .map_err(|e| format!("Cannot inspect {}: {e}", part.display()))?;
                if meta.file_type().is_symlink() || !meta.is_dir() {
                    return Err(format!("Unsafe Matrox installation directory: {}", part.display()));
                }
            }
        }
        let path = folder.join(relative);
        let meta = regular_not_symlink(&path)?;
        if meta.len() != expected_size {
            return Err(format!("Matrox OEM size mismatch: {}", path.display()));
        }
        let mut file = File::open(&path)
            .map_err(|e| format!("Cannot open {}: {e}", path.display()))?;
        let mut hasher = Sha256::new();
        let mut buffer = [0u8; 65536];
        loop {
            let count = file.read(&mut buffer)
                .map_err(|e| format!("Cannot read {}: {e}", path.display()))?;
            if count == 0 {
                break;
            }
            hasher.update(&buffer[..count]);
        }
        if format!("{:x}", hasher.finalize()) != expected_sha {
            return Err(format!("Matrox OEM SHA-256 mismatch: {}", path.display()));
        }
        checked += 1;
        bytes += expected_size;
    }
    Ok(VerifiedOem { files: checked, bytes })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn manifest_matches_verified_oem_executable_and_all_files() {
        let e = manifest_entries().unwrap();
        assert_eq!(e.len(), 66);
        assert_eq!(e.iter().map(|row| row.1).sum::<u64>(), OEM_TOTAL_BYTES);
        assert_eq!(
            e.iter().find(|row| row.2 == "splanetw.exe").unwrap().0,
            ORIGINAL_EXE
        );
    }

    #[test]
    fn read_only_validation_rejects_missing_originals() {
        let missing = std::env::temp_dir()
            .join(format!("matrox-missing-{}", std::process::id()));
        assert!(verify_original(&missing).is_err());
    }
}
