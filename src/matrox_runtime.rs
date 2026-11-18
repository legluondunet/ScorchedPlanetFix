// SPDX-License-Identifier: GPL-3.0-or-later
//! Experimental Matrox Mystique OEM backend. No original game files are edited.
//! This module is deliberately independent of the 3dfx/GameFrame path.
use crate::matrox;
use sha2::{Digest, Sha256};
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, Command};

const OEM_FOLDER: &str = "scorchedplanet-matrox";
const RUNTIME_FOLDER: &str = "scorchedplanet-matrox-runtime";
const STAGE_FOLDER: &str = ".scorchedplanet-matrox-runtime-staging";
const DLL_SOURCE_NAME: &str = "ScorchedPlanetFix_MSI95_v035.dll";
const DLL_SHA: &str = "0d31c8f95e0387c959a59c57d57f29f4b599cd9b3469b0e2fce3383982a3946e";
const PATCHED_EXE_SHA: &str = "333de76b4cf4d358fd86cc1abbf5eda0ac95a101a23053d2764a00275f127be6";
const PATCH_MARKER: &str = "scorchedplanetfix-matrox-oem-runtime-1\n";
const WIN95_OFFSET: usize = 0x1579b;
const FADELOW_OFFSET: usize = 0x6ec8b;

fn safe_regular(path: &Path) -> Result<std::fs::Metadata, String> {
    let meta = fs::symlink_metadata(path)
        .map_err(|e| format!("Cannot inspect {}: {e}", path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink() {
        return Err(format!("Refusing non-regular or symlinked Matrox file: {}", path.display()));
    }
    Ok(meta)
}

fn safe_directory(path: &Path) -> Result<(), String> {
    let meta = fs::symlink_metadata(path)
        .map_err(|e| format!("Cannot inspect {}: {e}", path.display()))?;
    if !meta.is_dir() || meta.file_type().is_symlink() {
        return Err(format!("Refusing non-directory or symlinked Matrox path: {}", path.display()));
    }
    Ok(())
}

fn hash_file(path: &Path) -> Result<String, String> {
    safe_regular(path)?;
    let mut src = File::open(path).map_err(|e| format!("Cannot open {}: {e}", path.display()))?;
    let mut hash = Sha256::new();
    let mut buffer = [0u8; 65536];
    loop {
        let n = src.read(&mut buffer)
            .map_err(|e| format!("Cannot read {}: {e}", path.display()))?;
        if n == 0 { break; }
        hash.update(&buffer[..n]);
    }
    Ok(format!("{:x}", hash.finalize()))
}

fn digest(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}

fn patch_executable(original: &Path, target: &Path) -> Result<(), String> {
    if safe_regular(original)?.len() != 840_704 {
        return Err("Unknown Matrox executable length; patch refused".into());
    }
    let mut exe = fs::read(original).map_err(|e| format!("Read original OEM executable: {e}"))?;
    if digest(&exe) != "1f2684205f4aa349183acfef6eb660e92e6bb24732e5c56278f0df1111108894" {
        return Err("Original Matrox executable SHA-256 mismatch; patch refused".into());
    }
    if exe[WIN95_OFFSET..WIN95_OFFSET + 2] != [0x74, 0x1d]
        || exe[FADELOW_OFFSET..FADELOW_OFFSET + 4] != [0x17, 0xb7, 0xd1, 0x38] {
        return Err("Unknown Matrox Win95/FadeLow patch sites; patch refused".into());
    }
    exe[WIN95_OFFSET] = 0xeb;
    exe[FADELOW_OFFSET..FADELOW_OFFSET + 4].copy_from_slice(&[0xac, 0xc5, 0x27, 0x37]);
    if digest(&exe) != PATCHED_EXE_SHA {
        return Err("Patched Matrox executable does not match known SHA-256".into());
    }
    let mut dest = OpenOptions::new().write(true).create_new(true).open(target)
        .map_err(|e| format!("Cannot create isolated Matrox executable: {e}"))?;
    dest.write_all(&exe).map_err(|e| e.to_string())?;
    dest.sync_all().map_err(|e| e.to_string())?;
    Ok(())
}

/// Verifies every copied file. A save/config file may be modified by gameplay,
/// but original OEM data, the patched EXE, and the clean-room DLL must match.
pub fn verify_prepared(root: &Path) -> Result<(), String> {
    matrox::verify_original(&root.join(OEM_FOLDER))?;
    let runtime = root.join(RUNTIME_FOLDER);
    safe_directory(&runtime)?;
    let marker = runtime.join(".scorchedplanetfix-matrox");
    if safe_regular(&marker)?.len() != PATCH_MARKER.len() as u64
        || fs::read(&marker).map_err(|e| e.to_string())? != PATCH_MARKER.as_bytes() {
        return Err("Matrox runtime marker absent or invalid".into());
    }
    for (original_sha, bytes, rel) in matrox::manifest_entries()? {
        let expected = match rel {
            "splanetw.exe" => PATCHED_EXE_SHA,
            "msi95.dll" => DLL_SHA,
            _ => original_sha
        };
        let path = runtime.join(rel);
        for parent in Path::new(rel).parent().into_iter().flat_map(|p| p.ancestors()).filter(|p| p != &Path::new("")) {
            safe_directory(&runtime.join(parent))?;
        }
        let meta = safe_regular(&path)?;
        if rel != "msi95.dll" && meta.len() != bytes {
            return Err(format!("Matrox runtime file length changed: {}", path.display()));
        }
        if hash_file(&path)? != expected {
            return Err(format!("Matrox runtime SHA-256 mismatch: {}", path.display()));
        }
    }
    let ini = runtime.join("splanet.ini");
    if safe_regular(&ini)?.len() > 1_048_576 {
        return Err("Matrox runtime INI exceeds safe size limit".into());
    }
    Ok(())
}

/// Stages a complete isolated runtime and publishes it only after verification.
/// The caller must supply the exact clean-room MSI95 v0.35 DLL alongside the
/// launcher; this temporary requirement will be removed when CI embeds MSI95.
pub fn prepare(root: &Path) -> Result<PathBuf, String> {
    let original = root.join(OEM_FOLDER);
    matrox::verify_original(&original)?;
    let runtime = root.join(RUNTIME_FOLDER);
    if fs::symlink_metadata(&runtime).is_ok() {
        verify_prepared(root)?;
        return Ok(runtime);
    }
    let provided_dll = root.join(DLL_SOURCE_NAME);
    if hash_file(&provided_dll)? != DLL_SHA {
        return Err(format!("Expected the clean-room MSI95 v0.35 DLL (SHA-256 {}) at {}; original proprietary msi95.dll is not permitted", DLL_SHA, provided_dll.display()));
    }
    let stage = root.join(STAGE_FOLDER);
    // Never remove a pre-existing staging path: it may contain user data.
    if fs::symlink_metadata(&stage).is_ok() {
        return Err(format!("Pre-existing Matrox staging path; manual inspection required: {}", stage.display()));
    }
    fs::create_dir(&stage).map_err(|e| format!("Cannot stage Matrox runtime: {e}"))?;
    let result = (|| -> Result<(), String> {
        for (_sha, _bytes, rel) in matrox::manifest_entries()? {
            let target = stage.join(rel);
            if let Some(dir) = target.parent() {
                fs::create_dir_all(dir).map_err(|e| e.to_string())?;
            }
            match rel {
                "splanetw.exe" => patch_executable(&original.join(rel), &target)?,
                "msi95.dll" => {
                    fs::copy(&provided_dll, &target)
                        .map_err(|e| format!("Cannot copy clean-room MSI95 DLL: {e}"))?;
                },
                _ => {
                    fs::copy(original.join(rel), &target)
                        .map_err(|e| format!("Cannot copy original Matrox data {rel}: {e}"))?;
                }
            }
        }
        let ini = original.join("splanet.ini");
        let size = safe_regular(&ini)?.len();
        if size > 1_048_576 {
            return Err("Original Matrox INI is too large".into());
        }
        fs::copy(ini, stage.join("splanet.ini")).map_err(|e| format!("Cannot copy Matrox INI: {e}"))?;
        fs::write(stage.join(".scorchedplanetfix-matrox"), PATCH_MARKER)
            .map_err(|e| e.to_string())?;
        // Audit stage in place before publication.
        for (sha, bytes, rel) in matrox::manifest_entries()? {
            let expect = match rel {
                "splanetw.exe" => PATCHED_EXE_SHA,
                "msi95.dll" => DLL_SHA,
                _ => sha,
            };
            let path = stage.join(rel);
            if rel != "msi95.dll" && safe_regular(&path)?.len() != bytes {
                return Err(format!("Staged Matrox file size mismatch: {rel}"));
            }
            if hash_file(&path)? != expect {
                return Err(format!("Staged Matrox data verification failed: {rel}"));
            }
        }
        if fs::symlink_metadata(&runtime).is_ok() {
            return Err("Matrox runtime destination appeared during staging; refusing to overwrite".into());
        }
        fs::rename(&stage, &runtime)
            .map_err(|e| format!("Cannot publish Matrox runtime: {e}"))?;
        Ok(())
    })();
    if let Err(ref e) = result {
        // Only the freshly created private stage is ours to delete; we never
        // remove an existing installation, any existing stage or user save.
        if !runtime.exists() {
            let _ = fs::remove_dir_all(&stage);
        }
        return Err(e.clone());
    }
    verify_prepared(root)?;
    Ok(runtime)
}

fn validated_env(command: &mut Command) {
    // Make flags deterministic: never inherit legacy experimental modes.
    let controlled = [
        "MATROX_MSI_BORDERLESS", "MATROX_MSI_PERSPECTIVE",
        "MATROX_MSI_INDEXED_KEY", "MATROX_MSI_SHADOW_ONCE",
        "MATROX_MSI_MODULATE", "MATROX_MSI_DITHER",
        "MATROX_MSI_SHADOW_BLEND", "MATROX_MSI_SHADOW_UNDERLAY",
        "MATROX_MSI_WORLD_BLUE_KEY",
    ];
    for name in controlled {
        command.env_remove(name);
    }
    for name in [
        "MATROX_MSI_BORDERLESS", "MATROX_MSI_PERSPECTIVE",
        "MATROX_MSI_INDEXED_KEY", "MATROX_MSI_SHADOW_ONCE", "MATROX_MSI_MODULATE",
    ] {
        command.env(name, "1");
    }
}

/// Normal gameplay is not subject to the standalone 300-second diagnostic
/// timeout. The v0.35 MSI95 driver itself keeps the hard 60 FPS ceiling.
pub fn spawn(root: &Path) -> Result<Child, String> {
    verify_prepared(root)?;
    let game = root.join(RUNTIME_FOLDER);
    let exe = game.join("splanetw.exe");
    let mut command = Command::new(&exe);
    command.current_dir(&game);
    validated_env(&mut command);
    command.spawn().map_err(|e| format!("Cannot launch Matrox Mystique OEM: {e}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn patch_offsets_and_known_digests_are_stable() {
        assert_eq!(WIN95_OFFSET, 0x1579b);
        assert_eq!(FADELOW_OFFSET, 0x6ec8b);
        assert_eq!(PATCHED_EXE_SHA.len(), 64);
        assert_eq!(DLL_SHA.len(), 64);
        assert_eq!(matrox::manifest_entries().unwrap().len(), 66);
    }
    #[test]
    fn preparation_does_not_create_anything_without_recognized_originals() {
        let missing = std::env::temp_dir()
            .join(format!("matrox-missing-runtime-{}", std::process::id()));
        assert!(prepare(&missing).is_err());
        assert!(!missing.join(RUNTIME_FOLDER).exists());
    }
}
