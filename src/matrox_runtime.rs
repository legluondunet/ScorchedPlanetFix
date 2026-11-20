// SPDX-License-Identifier: GPL-3.0-or-later
//! One Matrox Mystique OEM game installation in game/, sharing the standard
//! CrimsonSkiesFix-style layout with retail 3dfx. No second runtime folder.
//! The exact original OEM EXE and MSI95 are preserved as .bak files.
use crate::{matrox, matrox_iso, settings};
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

/// Publish a new installer-owned MSI95 driver without a permanent backup.
/// The short-lived '.replacing' file exists only during an atomic rollback
/// transaction, and is deleted after successful verification. It is NEVER
/// the original Matrox OEM driver: that separately lives in msi95.dll.bak.
fn replace_owned_driver(game:&Path,desired:&[u8],expected:&str,
                        mut verify_after:impl FnMut()->Result<(),String>)
    ->Result<(),String>{
    let current=game.join("msi95.dll");
    let staged=game.join(".msi95.dll.installing");
    let displaced=game.join(".msi95.dll.replacing");
    for item in [&staged,&displaced]{
        if fs::symlink_metadata(item).is_ok(){
            return Err(format!("Interrupted MSI95 update; inspect {}",item.display()));
        }
    }
    let old_exists=match fs::symlink_metadata(&current){
        Ok(_)=>{safe_regular(&current)?;true},
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>false,
        Err(e)=>return Err(format!("Cannot inspect MSI95 driver {}: {e}",current.display())),
    };
    if let Err(e)=write_new(&staged,desired){
        let _=fs::remove_file(&staged);return Err(e);
    }
    if hash_file(&staged)?!=expected{
        let _=fs::remove_file(&staged);
        return Err("Staged MSI95 checksum mismatch".into());
    }
    if old_exists {
        if let Err(e)=fs::rename(&current,&displaced){
            let _=fs::remove_file(&staged);
            return Err(format!("Cannot stage previous MSI95 driver for rollback: {e}"));
        }
    }
    if let Err(e)=fs::rename(&staged,&current){
        if old_exists{let _=fs::rename(&displaced,&current);}
        let _=fs::remove_file(&staged);
        return Err(format!("Cannot publish installer-owned MSI95 DLL: {e}"));
    }
    if let Err(e)=verify_after(){
        let _=fs::remove_file(&current);
        if old_exists{let _=fs::rename(&displaced,&current);}
        return Err(format!("MSI95 update verification failed; previous driver restored if present: {e}"));
    }
    if old_exists {
        fs::remove_file(&displaced)
            .map_err(|e|format!("MSI95 upgraded but temporary rollback remains: {e}"))?;
    }
    Ok(())
}

/// A working, hash-locked OEM installation may use a prior build of OUR OWN
/// open-source DLL. The bundled MSI95 is the installer's file: repair/update
/// it automatically, without needing a whitelist for each CI binary hash.
/// This is safe only AFTER all 66 original resources + original OEM driver
/// and EXE backups and the patched EXE are independently authenticated.
/// Never change a custom game executable, original backup, or user resources.
pub fn prepare(root:&Path,mut log:impl FnMut(&str))->Result<PathBuf,String>{
    let game=root.join(GAME_FOLDER);
    if let Err(current_error)=verify_prepared(root){
        matrox::verify_installed_game(&game,PATCHED_EXE_SHA)
            .map_err(|e|format!("Cannot update MSI95: original game authentication failed: {e}"))?;
        check_ini(&game.join("splanet.ini"))?;
        let current=game.join("msi95.dll");
        let driver_current=match fs::symlink_metadata(&current){
            Ok(_)=>Some(hash_file(&current)?),
            Err(e) if e.kind()==std::io::ErrorKind::NotFound=>None,
            Err(e)=>return Err(format!("Cannot inspect MSI95: {e}")),
        };
        // If the bundled driver already matches, do not attempt a spurious
        // repair. The original verification error must then be reported.
        if driver_current.as_deref()==Some(expected_driver_hash()){
            return Err(current_error);
        }
        let replacement=cleanroom_driver(root)?;
        log(&format!(
            "MSI95: original OEM files and patched EXE authenticated; updating installer-owned driver from {} to {}.",
            driver_current.as_deref().unwrap_or("(missing)"),expected_driver_hash()));
        replace_owned_driver(&game,&replacement,expected_driver_hash(),
                             ||verify_prepared(root))?;
        log("MSI95: automatic update verified; original game/msi95.dll.bak preserved; no new permanent backup.");
    }
    matrox_iso::update_generated_ini(&game)?;
    settings::ensure_defaults(&root.join("ScorchedPlanetFix.ini"),true)?;
    Ok(game)
}

fn configure(command: &mut Command,root:&Path,force_first_log:bool)->Result<(),String> {
    let configured=settings::read_settings(&root.join("ScorchedPlanetFix.ini"))?;
    let controlled=[
        "MATROX_MSI_BORDERLESS","MATROX_MSI_PERSPECTIVE",
        "MATROX_MSI_INDEXED_KEY","MATROX_MSI_SHADOW_ONCE",
        "MATROX_MSI_MODULATE","MATROX_MSI_DITHER","MATROX_MSI_FPS_LIMIT","MATROX_MSI_LOG",
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
    command.env("MATROX_MSI_LOG",if configured.log || force_first_log{"1"}else{"0"});
    Ok(())
}
pub fn spawn(root: &Path,force_first_log:bool)->Result<Child,String> {
    verify_prepared(root)?;
    spawn_impl(root,force_first_log)
}
pub fn spawn_unchecked(root: &Path)->Result<Child,String> {
    spawn_impl(root,false)
}
fn spawn_impl(root: &Path,force_first_log:bool)->Result<Child,String> {
    let game=root.join(GAME_FOLDER);
    let mut command=Command::new(game.join("splanetw.exe"));
    command.current_dir(&game);
    configure(&mut command,root,force_first_log)?;
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
        configure(&mut c,&root,false).unwrap();
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_FPS_LIMIT" &&
            v==Some(std::ffi::OsStr::new("120"))));
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_DITHER" &&
            v==Some(std::ffi::OsStr::new("1"))));
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_LOG" &&
            v==Some(std::ffi::OsStr::new("0"))));
        fs::write(root.join("ScorchedPlanetFix.ini"),b"[Debug]\nLog=1\n").unwrap();
        configure(&mut c,&root,false).unwrap();
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_LOG" &&
            v==Some(std::ffi::OsStr::new("1"))));
        fs::write(root.join("ScorchedPlanetFix.ini"),b"[Debug]\nLog=0\n").unwrap();
        configure(&mut c,&root,true).unwrap();
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_LOG" &&
            v==Some(std::ffi::OsStr::new("1"))));
        configure(&mut c,&root,false).unwrap();
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_LOG" &&
            v==Some(std::ffi::OsStr::new("0"))));
        fs::remove_dir_all(&root).unwrap();
    }
    #[test]
    fn replacing_installer_driver_never_leaves_a_permanent_backup(){
        let root=std::env::temp_dir().join(format!("msi95-upgrade-test-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let active=root.join("msi95.dll");
        let old=b"earlier trusted installer driver";
        let replacement=b"new bundled driver";
        let expected=digest(replacement);
        fs::write(&active,old).unwrap();
        replace_owned_driver(&root,replacement,&expected,
            ||{if hash_file(&active)?==expected{Ok(())}
               else{Err("unexpected new driver".into())}}).unwrap();
        assert_eq!(fs::read(&active).unwrap(),replacement);
        assert!(!root.join(".msi95.dll.replacing").exists());
        assert!(!root.join(".msi95.dll.installing").exists());
        assert!(!root.join("msi95.dll.bak").exists(),
            "The installer-supplied DLL must never receive a permanent .bak");
        // If the verification fails, roll back in place, without losing the
        // previous DLL or creating a permanent backup.
        let err=replace_owned_driver(&root,b"another variant",&digest(b"another variant"),
            ||Err("artificial verification rejection".into())).unwrap_err();
        assert!(err.contains("artificial verification rejection"));
        assert_eq!(fs::read(&active).unwrap(),replacement);
        assert!(!root.join(".msi95.dll.replacing").exists());
        fs::remove_file(&active).unwrap();
        replace_owned_driver(&root,replacement,&expected,||Ok(())).unwrap();
        assert_eq!(fs::read(&active).unwrap(),replacement);
        assert!(!root.join("msi95.dll.bak").exists());
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn interrupted_staging_refuses_to_overwrite_any_file(){
        let root=std::env::temp_dir().join(format!("msi95-staging-test-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let dll=root.join("msi95.dll");
        fs::write(&dll,b"old-contents").unwrap();
        fs::write(root.join(".msi95.dll.installing"),b"unresolved-stage").unwrap();
        assert!(replace_owned_driver(&root,b"new", &digest(b"new"),||Ok(())).is_err());
        assert_eq!(fs::read(&dll).unwrap(),b"old-contents");
        assert_eq!(fs::read(root.join(".msi95.dll.installing")).unwrap(),b"unresolved-stage");
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn missing_game_does_not_create_second_runtime(){
        let root=std::env::temp_dir().join(format!("matrox-missing-{}",std::process::id()));
        assert!(prepare(&root,|_|{}).is_err());
        assert!(!root.join(GAME_FOLDER).exists());
    }
}
