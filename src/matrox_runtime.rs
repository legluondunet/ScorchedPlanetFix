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

/// The active MSI95 is installer-managed: its current hash is used ONLY
/// to detect equality with the embedded DLL, never as a recognition/allowlist
/// test. Even an arbitrary, unrecognized DLL is replaced. This helper must
/// only be called AFTER authenticating the original game and OEM backups.
fn ensure_owned_driver(
    game:&Path,expected:&str,
    load_embedded:impl FnOnce()->Result<Vec<u8>,String>,
    mut log:impl FnMut(&str),
    verify_after:impl FnMut()->Result<(),String>,
)->Result<(),String>{
    let active=game.join("msi95.dll");
    let current_hash=match fs::symlink_metadata(&active){
        Ok(_)=>Some(hash_file(&active)?),
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>None,
        Err(e)=>return Err(format!("Cannot inspect MSI95 DLL {}: {e}",active.display())),
    };
    // Identical content: do not even load or rewrite the DLL. Preserves
    // timestamps, avoids transient file locks and never opens a rollback.
    if current_hash.as_deref()==Some(expected){
        log("MSI95: identical to bundled DLL (SHA-256), leaving file untouched.");
        return Ok(());
    }
    let new_dll=load_embedded()?;
    if digest(&new_dll)!=expected{
        return Err("Installer-provided MSI95 DLL does not match expected SHA-256".into());
    }
    log(&format!(
        "MSI95: installing embedded DLL: current={} bundled={} (no old-DLL recognition required).",
        current_hash.as_deref().unwrap_or("(missing)"),expected));
    replace_owned_driver(game,&new_dll,expected,verify_after)?;
    log("MSI95: embedded DLL installed and verified; temporary rollback removed; original OEM msi95.dll.bak unchanged.");
    Ok(())
}

/// The launcher owns game/msi95.dll, whether it already contains a former
/// clean-room build or an unknown DLL. Its current hash is only compared
/// against the hash of the embedded driver. The OEM game (including the
/// original proprietary msi95.dll.bak and the patched EXE) is verified
/// independently before any changes to the active renderer.
pub fn prepare(root:&Path,mut log:impl FnMut(&str))->Result<PathBuf,String>{
    let game=root.join(GAME_FOLDER);
    matrox::verify_installed_game(&game,PATCHED_EXE_SHA)
        .map_err(|e|format!("Cannot prepare Matrox MSI95: original game authentication failed: {e}"))?;
    check_ini(&game.join("splanet.ini"))?;
    ensure_owned_driver(&game,expected_driver_hash(),||cleanroom_driver(root),
        |message|log(message),||verify_prepared(root))?;
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
    fn installed_driver_is_replaced_even_when_unrecognized_and_identical_is_untouched(){
        let root=std::env::temp_dir().join(format!("msi95-unconditional-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let active=root.join("msi95.dll");
        let embedded=b"the exact installer-owned DLL";
        let expected=digest(embedded);
        // Absolutely arbitrary bytes. No legacy-driver fingerprint or PE header.
        fs::write(&active,b"unrecognized third-party MSI95 payload").unwrap();
        let mut messages=Vec::new();
        ensure_owned_driver(&root,&expected,||Ok(embedded.to_vec()),
            |line|messages.push(line.to_string()),
            ||if hash_file(&active)?==expected{Ok(())}else{Err("bad update".into())}).unwrap();
        assert_eq!(fs::read(&active).unwrap(),embedded);
        assert!(messages.iter().any(|line|line.contains("no old-DLL recognition required")));
        assert!(!root.join(".msi95.dll.replacing").exists());
        assert!(!root.join("msi95.dll.bak").exists());

        // Exact bundled DLL => no file modification, no loader invocation,
        // and no touching a pre-existing transaction marker.
        let blocked=root.join(".msi95.dll.installing");
        fs::write(&blocked,b"do not modify").unwrap();
        ensure_owned_driver(&root,&expected,
            ||Err("identical driver was needlessly loaded".into()),
            |_|{},||Err("identical driver was needlessly reverified".into())).unwrap();
        assert_eq!(fs::read(&active).unwrap(),embedded);
        assert_eq!(fs::read(&blocked).unwrap(),b"do not modify");
        fs::remove_file(&blocked).unwrap();

        // Also repair a missing active driver, without creating a backup.
        fs::remove_file(&active).unwrap();
        ensure_owned_driver(&root,&expected,||Ok(embedded.to_vec()),|_|{},
            ||if hash_file(&active)?==expected{Ok(())}else{Err("bad repair".into())}).unwrap();
        assert_eq!(fs::read(&active).unwrap(),embedded);
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
