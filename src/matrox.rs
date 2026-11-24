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
const RETAIL_MANIFEST: &str = include_str!("../data/cd-inventories/scorched-matrox-fr-de-alt.sha256");
pub(crate) const RETAIL_ORIGINAL_EXE: &str = "7865e8a94a6b2b44511943a1bd2801ece302eee31444ee8a70152ec483a54748";
pub(crate) const RETAIL_PATCHED_EXE: &str = "5e5496b4b3d3424fb717ac41708403aebab3b3dec653c40cf32de0dc9cd750b6";

/// Retail selection is separate from OEM: original executable name is preserved.
pub(crate) fn is_retail(folder: &Path) -> bool {
    fs::symlink_metadata(folder.join("spmyst.exe")).is_ok() ||
        fs::symlink_metadata(folder.join("spmyst.exe.bak")).is_ok()
}
pub(crate) fn exe_name(folder: &Path) -> &'static str {
    if is_retail(folder) {"spmyst.exe"} else {"splanetw.exe"}
}
pub(crate) fn backup_name(folder: &Path) -> &'static str {
    if is_retail(folder) {"spmyst.exe.bak"} else {"splanetw.exe.bak"}
}
pub(crate) fn retail_entries() -> Result<Vec<(&'static str,u64,&'static str,&'static str)>,String> {
    let mut entries=Vec::new();
    for row in RETAIL_MANIFEST.lines().map(str::trim).filter(|x| !x.is_empty() && !x.starts_with('#')) {
        let mut fields=row.split_whitespace();
        let sha=fields.next().ok_or("Missing retail SHA")?;
        let size=fields.next().ok_or("Missing retail size")?.parse::<u64>().map_err(|_|"Invalid retail size")?;
        let source=fields.next().ok_or("Missing retail path")?;
        let (tree,dest)=source.split_once('/').ok_or("Invalid retail source")?;
        if fields.next().is_some() || !matches!(tree,"splanet"|"mystique") ||
            sha.len()!=64 || !sha.bytes().all(|b|b.is_ascii_hexdigit()&&!b.is_ascii_uppercase()) ||
            !dest.is_ascii() || dest.contains('\\') ||
            Path::new(dest).components().any(|c|!matches!(c,Component::Normal(_))) ||
            entries.iter().any(|&(_,_,_,p)|p==dest) {
            return Err("Invalid retail Matrox manifest entry".into());
        }
        entries.push((sha,size,source,dest));
    }
    if entries.len()!=125 || entries.iter().map(|x|x.1).sum::<u64>()!=499_674_217 ||
       !entries.iter().any(|x|x.3=="spmyst.exe" && x.0==RETAIL_ORIGINAL_EXE) ||
       !entries.iter().any(|x|x.3=="msi95.dll" && x.0=="6c219a0dc25a7c948e2973ac99f8b900696ad87aeb2a08a6b162f96b5bc07820") {
        return Err("Incomplete retail Matrox manifest".into());
    }
    Ok(entries)
}
pub(crate) fn profile_entries(retail:bool) -> Result<Vec<(&'static str,u64,&'static str)>,String> {
    if retail {Ok(retail_entries()?.into_iter().map(|(h,n,_,p)|(h,n,p)).collect())}
    else {manifest_entries()}
}
const OEM_FILES: usize = 66;
const OEM_TOTAL_BYTES: u64 = 315_947_520;
const ORIGINAL_EXE: &str =
    "1f2684205f4aa349183acfef6eb660e92e6bb24732e5c56278f0df1111108894";
pub(crate) const ORIGINAL_MSI95: &str =
    "71b9921d8225403299fc73932a49ce5d3c1261a3bf7a852a3659d3f81cee3d9e";

#[derive(Debug, PartialEq, Eq)]
pub struct VerifiedOem {
    pub files: usize,
    pub bytes: u64,
}

pub(crate) fn manifest_entries() -> Result<Vec<(&'static str, u64, &'static str)>, String> {
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

/// Authenticate all immutable installed OEM resources and the original EXE
/// backup. The original OEM driver is authenticated at ISO extraction time
/// and intentionally DISCARDED on install: no DLL backup is needed. The
/// installed audit checks 65 original resources (of 66 entries on the CD),
/// excluding the installer-owned active MSI95 DLL.
pub fn verify_installed_oem(folder:&Path)->Result<VerifiedOem,String>{
    verify_reference(folder,true)
}

/// Authenticate the immutable installed game files (65 CD resources plus
/// the original EXE backup, excluding the replaced OEM driver) and the
/// installer-patched game EXE. The active MSI95.dll is verified separately.
pub fn verify_installed_game(folder: &Path, patched_exe_hash: &str)
    -> Result<VerifiedOem, String> {
    let report = verify_reference(folder, true)?;
    verify_modified(folder,exe_name(folder),patched_exe_hash)?;
    Ok(report)
}

fn verify_modified(folder:&Path,relative:&str,hash:&str)->Result<(),String>{
    let path=folder.join(relative);
    regular_not_symlink(&path)?;
    let bytes=fs::read(&path).map_err(|e|e.to_string())?;
    if format!("{:x}",Sha256::digest(&bytes))!=hash{
        return Err(format!("Modified or unsupported Matrox runtime: {}",path.display()));
    }
    Ok(())
}

pub fn verify_installed(folder: &Path, patched_exe_hash: &str, driver_hash: &str)
    -> Result<VerifiedOem, String> {
    let report=verify_installed_game(folder,patched_exe_hash)?;
    verify_modified(folder,"msi95.dll",driver_hash)?;
    Ok(report)
}

/// Read-only SHA-256 audit: 66 original files on CD; 65 immutable original
/// files on the installed game (original MSI95.dll replaced and discarded).
/// The user's splanet.ini and future saves are deliberately excluded.
 /// All intermediate directories are checked for symlinks.
pub fn verify_original(folder: &Path) -> Result<VerifiedOem, String> {
    verify_reference(folder,false)
}

fn verify_reference(folder: &Path, installed: bool) -> Result<VerifiedOem, String> {
    let root_meta = fs::symlink_metadata(folder)
        .map_err(|e| format!("Cannot inspect Matrox installation: {e}"))?;
    if !root_meta.is_dir() || root_meta.file_type().is_symlink() {
        return Err(format!("Matrox game directory is invalid or symlinked: {}", folder.display()));
    }
    let retail=is_retail(folder);
    let entries = profile_entries(retail)?;
    let mut checked = 0;
    let mut bytes = 0;
    for (expected_sha, expected_size, rel) in entries {
        // The original proprietary MSI95 driver is verified on the user's
        // ISO before extraction. Once installed it is no longer needed,
        // and there is intentionally no game/msi95.dll.bak to verify.
        if installed && (rel=="msi95.dll" || (retail && rel.ends_with(".sav"))) {continue;}
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
        let path = if installed && rel==exe_name(folder) {
            folder.join(backup_name(folder))
        } else { folder.join(relative) };
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
    fn retail_plan_has_one_executable_and_no_software_assets(){
        let entries=retail_entries().unwrap();
        assert_eq!(entries.len(),125);
        assert_eq!(entries.iter().filter(|x|x.3.ends_with(".exe")).count(),1);
        assert_eq!(entries.iter().filter(|x|x.3.ends_with(".sav")).count(),8);
        assert!(entries.iter().any(|x|x.2=="mystique/french/mapsm.rwb"));
        assert!(entries.iter().any(|x|x.2=="splanet/french/brief.rwb"));
        assert!(!entries.iter().any(|x|x.3.ends_with("8.rwb")||x.3.ends_with("6.rwb")));
    }
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
    fn installed_audit_excludes_original_oem_dll_but_keeps_exe_backup(){
        let entries=manifest_entries().unwrap();
        assert_eq!(entries.iter().filter(|entry|entry.2!="msi95.dll").count(),65);
        assert!(entries.iter().any(|e|e.2=="splanetw.exe" && e.0==ORIGINAL_EXE));
        assert!(entries.iter().any(|e|e.2=="msi95.dll" && e.0==ORIGINAL_MSI95),
            "Full original ISO fingerprint must remain strict");
    }
    #[test]
    fn read_only_validation_rejects_missing_originals() {
        let missing = std::env::temp_dir()
            .join(format!("matrox-missing-{}", std::process::id()));
        assert!(verify_original(&missing).is_err());
    }
}
