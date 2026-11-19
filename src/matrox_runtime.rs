// SPDX-License-Identifier: GPL-3.0-or-later
//! One Matrox Mystique OEM game installation in game/, sharing the standard
//! CrimsonSkiesFix-style layout with retail 3dfx. No second runtime folder.
//! The exact original OEM EXE and MSI95 are preserved as .bak files.
use crate::{matrox, settings};
use sha2::{Digest,Sha256};
use std::fs::{self,File,OpenOptions};
use std::io::{Read,Write};
use std::path::{Path,PathBuf};
use std::process::{Child,Command};

const GAME_FOLDER: &str = "game";
const DRIVER_SOURCE: &str = "ScorchedPlanetFix_MSI95_v035.dll";
const LEGACY_DRIVER_HASH: &str = "0d31c8f95e0387c959a59c57d57f29f4b599cd9b3469b0e2fce3383982a3946e";
const PATCHED_EXE_SHA: &str = "333de76b4cf4d358fd86cc1abbf5eda0ac95a101a23053d2764a00275f127be6";
const ORIGINAL_EXE_SHA: &str = "1f2684205f4aa349183acfef6eb660e92e6bb24732e5c56278f0df1111108894";
const WIN95_OFFSET: usize = 0x1579b;
const FADELOW_OFFSET: usize = 0x6ec8b;

#[cfg(matrox_embedded)]
const MSI95_DRIVER: &[u8] = include_bytes!(env!("SCORCHED_MSI95_DLL_PATH"));
#[cfg(not(matrox_embedded))]
const MSI95_DRIVER: &[u8] = &[];

fn expected_driver_hash() -> &'static str {
    option_env!("SCORCHED_MSI95_SHA256").unwrap_or(LEGACY_DRIVER_HASH)
}
fn digest(data: &[u8]) -> String {
    format!("{:x}",Sha256::digest(data))
}
fn safe_regular(path: &Path) -> Result<(),String> {
    let meta=fs::symlink_metadata(path).map_err(|e|format!("Cannot inspect {}: {e}",path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink() {
        return Err(format!("Not a regular Matrox file: {}",path.display()));
    }
    Ok(())
}
fn hash_file(path: &Path) -> Result<String,String> {
    safe_regular(path)?;
    let mut file=File::open(path).map_err(|e|e.to_string())?;
    let mut hasher=Sha256::new();
    let mut buf=[0u8;65536];
    loop {
        let n=file.read(&mut buf).map_err(|e|e.to_string())?;
        if n==0 {break;}
        hasher.update(&buf[..n]);
    }
    Ok(format!("{:x}",hasher.finalize()))
}
fn cleanroom_driver(root: &Path) -> Result<Vec<u8>,String> {
    let bytes=if MSI95_DRIVER.is_empty() {
        let path=root.join(DRIVER_SOURCE);
        safe_regular(&path)?;
        fs::read(&path).map_err(|e|e.to_string())?
    }else{MSI95_DRIVER.to_vec()};
    if bytes.len()<4096 || !bytes.starts_with(b"MZ") || digest(&bytes)!=expected_driver_hash() {
        return Err("Bundled MSI95 DLL is invalid or fails expected SHA-256".into());
    }
    Ok(bytes)
}
fn patched_exe(original: &Path) -> Result<Vec<u8>,String> {
    safe_regular(original)?;
    let mut exe=fs::read(original).map_err(|e|e.to_string())?;
    if exe.len()!=840_704 || digest(&exe)!=ORIGINAL_EXE_SHA {
        return Err("Unknown Matrox executable; patch refused".into());
    }
    if exe[WIN95_OFFSET..WIN95_OFFSET+2]!=[0x74,0x1d]
        || exe[FADELOW_OFFSET..FADELOW_OFFSET+4]!=[0x17,0xb7,0xd1,0x38] {
        return Err("Unknown Matrox Win95/FadeLow patch sites".into());
    }
    exe[WIN95_OFFSET]=0xeb;
    exe[FADELOW_OFFSET..FADELOW_OFFSET+4].copy_from_slice(&[0xac,0xc5,0x27,0x37]);
    if digest(&exe)!=PATCHED_EXE_SHA {
        return Err("Patched Matrox executable failed exact SHA-256 audit".into());
    }
    Ok(exe)
}
fn write_new(path: &Path,data: &[u8]) -> Result<(),String> {
    let mut output=OpenOptions::new().write(true).create_new(true).open(path)
        .map_err(|e|format!("Cannot create {}: {e}",path.display()))?;
    output.write_all(data).map_err(|e|e.to_string())?;
    output.sync_all().map_err(|e|e.to_string())
}
fn check_ini(path: &Path)->Result<(),String> {
    safe_regular(path)?;
    let len=fs::metadata(path).map_err(|e|e.to_string())?.len();
    if len>256*1024 {return Err("Matrox splanet.ini exceeds safe size limit".into());}
    Ok(())
}

/// The Matrox importer calls this in its PRIVATE, uncommitted staging directory.
/// Preserve all original OEM immutable files; rename the exact original EXE
/// and proprietary MSI95 to .bak, then put the verified replacements alongside
/// the game. No second runtime/ folder is generated.
pub fn prepare_staged(game: &Path, launcher_root: &Path)->Result<(),String>{
    matrox::verify_original(game)?;
    let driver=cleanroom_driver(launcher_root)?;
    let exe_path=game.join("splanetw.exe");
    let original_exe=game.join("splanetw.exe.bak");
    let original_driver=game.join("msi95.dll");
    let original_backup=game.join("msi95.dll.bak");
    for backup in [&original_exe,&original_backup] {
        if fs::symlink_metadata(backup).is_ok() {
            return Err(format!("Matrox backup already exists: {}",backup.display()));
        }
    }
    let exe=patched_exe(&exe_path)?;
    // All changes below affect only the private uncommitted stage.
    fs::rename(&exe_path,&original_exe).map_err(|e|e.to_string())?;
    write_new(&exe_path,&exe)?;
    fs::rename(&original_driver,&original_backup).map_err(|e|e.to_string())?;
    write_new(&original_driver,&driver)?;
    check_ini(&game.join("splanet.ini"))?;
    verify_prepared_at(game,expected_driver_hash())
}
fn verify_prepared_at(game: &Path,driver_hash: &str)->Result<(),String> {
    matrox::verify_installed(game,PATCHED_EXE_SHA,driver_hash)?;
    check_ini(&game.join("splanet.ini"))
}
pub fn verify_prepared(root: &Path)->Result<(),String> {
    verify_prepared_at(&root.join(GAME_FOLDER),expected_driver_hash())
}

/// Existing new-layout installations may have a previous known MSI95 renderer.
/// Replace ONLY an authenticated older driver with rollback, never touch
/// unknown DLLs, game data, player saves or the original MSI95 .bak backup.
pub fn prepare(root: &Path)->Result<PathBuf,String>{
    let game=root.join(GAME_FOLDER);
    if let Err(current_error)=verify_prepared(root) {
        if expected_driver_hash()==LEGACY_DRIVER_HASH {
            return Err(current_error);
        }
        verify_prepared_at(&game,LEGACY_DRIVER_HASH).map_err(|_|current_error)?;
        let replacement=cleanroom_driver(root)?;
        let current=game.join("msi95.dll");
        let staged=game.join(".msi95.dll.installing");
        let displaced=game.join(".msi95.dll.replacing");
        for item in [&staged,&displaced] {
            if fs::symlink_metadata(item).is_ok() {
                return Err(format!("Interrupted MSI95 upgrade; inspect {}",item.display()));
            }
        }
        write_new(&staged,&replacement)?;
        if hash_file(&staged)?!=expected_driver_hash() {
            let _=fs::remove_file(&staged);
            return Err("Staged MSI95 checksum mismatch".into());
        }
        if let Err(e)=fs::rename(&current,&displaced) {
            let _=fs::remove_file(&staged);
            return Err(format!("Cannot prepare MSI95 rollback: {e}"));
        }
        if let Err(e)=fs::rename(&staged,&current) {
            let _=fs::rename(&displaced,&current);
            let _=fs::remove_file(&staged);
            return Err(format!("Cannot publish new MSI95 driver: {e}"));
        }
        if let Err(e)=verify_prepared(root) {
            let _=fs::remove_file(&current);
            let _=fs::rename(&displaced,&current);
            return Err(format!("MSI95 update verification failed; original restored: {e}"));
        }
        fs::remove_file(&displaced)
            .map_err(|e|format!("MSI95 upgraded but stale rollback remains: {e}"))?;
    }
    settings::ensure_defaults(&root.join("ScorchedPlanetFix.ini"),true)?;
    Ok(game)
}

fn configure(command: &mut Command,root:&Path)->Result<(),String> {
    let configured=settings::read_settings(&root.join("ScorchedPlanetFix.ini"))?;
    let controlled=[
        "MATROX_MSI_BORDERLESS","MATROX_MSI_PERSPECTIVE",
        "MATROX_MSI_INDEXED_KEY","MATROX_MSI_SHADOW_ONCE",
        "MATROX_MSI_MODULATE","MATROX_MSI_DITHER","MATROX_MSI_FPS_LIMIT",
        "MATROX_MSI_SHADOW_BLEND","MATROX_MSI_SHADOW_UNDERLAY",
        "MATROX_MSI_WORLD_BLUE_KEY",
    ];
    for key in controlled {command.env_remove(key);}
    for key in ["MATROX_MSI_BORDERLESS","MATROX_MSI_PERSPECTIVE",
                "MATROX_MSI_INDEXED_KEY","MATROX_MSI_SHADOW_ONCE",
                "MATROX_MSI_MODULATE"] {
        command.env(key,"1");
    }
    command.env("MATROX_MSI_FPS_LIMIT",configured.fps_limit.to_string());
    command.env("MATROX_MSI_DITHER",if configured.dither{"1"}else{"0"});
    Ok(())
}
pub fn spawn(root: &Path)->Result<Child,String> {
    verify_prepared(root)?;
    spawn_unchecked(root)
}
pub fn spawn_unchecked(root: &Path)->Result<Child,String> {
    let game=root.join(GAME_FOLDER);
    let mut command=Command::new(game.join("splanetw.exe"));
    command.current_dir(&game);
    configure(&mut command,root)?;
    command.spawn().map_err(|e|format!("Cannot launch Matrox Mystique OEM: {e}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn offsets_manifest_and_driver_identity_are_stable(){
        assert_eq!(WIN95_OFFSET,0x1579b);
        assert_eq!(FADELOW_OFFSET,0x6ec8b);
        assert_eq!(matrox::manifest_entries().unwrap().len(),66);
        assert_eq!(PATCHED_EXE_SHA.len(),64);
    }
    #[test]
    fn environment_gets_shared_root_ini_values(){
        let root=std::env::temp_dir().join(format!("matrox-layout-env-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("ScorchedPlanetFix.ini"),
            b"[scorch]\nFPSLimit=120\n[matrox]\nDither=1\n").unwrap();
        let mut c=Command::new("unused");
        configure(&mut c,&root).unwrap();
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_FPS_LIMIT" &&
            v==Some(std::ffi::OsStr::new("120"))));
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_DITHER" &&
            v==Some(std::ffi::OsStr::new("1"))));
        fs::remove_dir_all(&root).unwrap();
    }
    #[test]
    fn missing_game_does_not_create_second_runtime(){
        let root=std::env::temp_dir().join(format!("matrox-missing-{}",std::process::id()));
        assert!(prepare(&root).is_err());
        assert!(!root.join(GAME_FOLDER).exists());
    }
}
