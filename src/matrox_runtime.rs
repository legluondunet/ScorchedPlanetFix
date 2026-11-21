// SPDX-License-Identifier: GPL-3.0-or-later
//! One Matrox Mystique OEM game installation in game/, sharing the standard
//! CrimsonSkiesFix-style layout with retail 3dfx. No second runtime folder.
//! Only the OEM EXE is preserved as .bak. No DLL backup is required.
use crate::{matrox, matrox_iso, settings};
use sha2::{Digest,Sha256};
use std::fs::{self,File,OpenOptions};
use std::io::{Read,Write};
use std::path::{Path,PathBuf};
use std::process::{Child,Command};

const GAME_FOLDER: &str = "game";
const DRIVER_SOURCE: &str = "ScorchedPlanetFix_MSI95_v035.dll";
const LEGACY_DRIVER_HASH: &str = "0d31c8f95e0387c959a59c57d57f29f4b599cd9b3469b0e2fce3383982a3946e";
const PATCHED_EXE_SHA: &str = "8bcded21d84b5d3cb8e4567bb32e6b038d168b632bdf081e4faebfcfdd50b710";
const PREVIOUS_NULL_GUARDED_EXE_SHA: &str = "52b5f604ad7037e29e8cc83244c86662a2e6896b1e4d024ac4c68f23d080a759";
const PREVIOUS_PATCHED_EXE_SHA: &str = "333de76b4cf4d358fd86cc1abbf5eda0ac95a101a23053d2764a00275f127be6";
/// Win32 PE .text VA 0x00401000 maps to file offset 0x400.
const NULL_GUARD_CAVE_OFFSET: usize = 0x93d53; // VA 0x00494953, 13 original CC bytes
const NULL_DEREF_OFFSET: usize = 0x93d65; // VA 0x00494965, mov eax,[esi+0x24]
const NULL_GUARD_CAVE: [u8;13] = [
    0x85,0xf6,             // test esi,esi
    0x0f,0x84,0x8e,0,0,0,  // je 0x004949e9 (original NULL-return branch)
    0x8b,0x46,0x24,        // mov eax,[esi+0x24]
    0xeb,0x08,             // jmp 0x00494968 (resume original function)
];
const NULL_GUARD_JUMP: [u8;3] = [0xeb,0xec,0x90]; // jmp 0x00494953; nop
/// Second reproducible crash: menu briefing clicked rapidly. The game's
/// object-pointer array contains a misaligned sentinel 0x00000002, which
/// was dereferenced by 0x004858CE as an object pointer. Only 4-byte aligned
/// entries are eligible for this object's +0x44 field access.
const BRIEFING_CAVE_OFFSET: usize = 0x84c53; // VA 0x00485853, 13 original CC bytes
const BRIEFING_DEREF_OFFSET: usize = 0x84cce; // VA 0x004858CE
const BRIEFING_CAVE: [u8;13] = [
    0xa8,0x03,       // test al,3 (object pointers must be DWORD aligned)
    0x74,0x04,       // jz valid
    0x31,0xc9,       // invalid entry => ecx=0 (not type 1)
    0xeb,0x76,       // resume original cmp ecx,1 -> skip this list entry
    0x8b,0x48,0x44, // valid => original mov ecx,[eax+0x44]
    0xeb,0x71,       // resume at 0x004858D1
];
const BRIEFING_GUARD_JUMP: [u8;3] = [0xeb,0x83,0x90]; // jump to 0x00485853
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
/// The original 1996 function 0x00494960 already has an intentional
/// NULL-input branch at 0x004949E9 (log error and return zero), but reads
/// [esi+0x24] at 0x00494965 BEFORE testing esi. In Wine logs we proved
/// esi=0 and its caller is 0x0042DB41. The following 16-byte, hash-locked
/// local patch performs the NULL test before dereferencing, then resumes
/// the original function for all non-NULL objects. The 13-byte code cave
/// was entirely CC padding in the exact authenticated OEM EXE.
fn apply_null_guard(exe:&mut [u8])->Result<(),String>{
    if exe.len()!=840_704
        || exe[NULL_GUARD_CAVE_OFFSET..NULL_GUARD_CAVE_OFFSET+13]!=[0xcc;13]
        || exe[NULL_DEREF_OFFSET..NULL_DEREF_OFFSET+3]!=[0x8b,0x46,0x24] {
        return Err("Unrecognized Matrox null-guard patch sites; refusing EXE modification".into());
    }
    exe[NULL_GUARD_CAVE_OFFSET..NULL_GUARD_CAVE_OFFSET+13]
        .copy_from_slice(&NULL_GUARD_CAVE);
    exe[NULL_DEREF_OFFSET..NULL_DEREF_OFFSET+3]
        .copy_from_slice(&NULL_GUARD_JUMP);
    Ok(())
}
/// Signature-locked defensive guard for malformed entries in the
/// briefing/scene object list. The reported EAX=2 is not an aligned pointer.
 /// This does not explain WHY an invalid entry appeared, but skips only
/// misaligned entries while preserving the original object-type checks.
/// The two branches were verified by disassembly of the exact OEM PE32.
fn apply_briefing_guard(exe:&mut [u8])->Result<(),String>{
    if exe.len()!=840_704
        || exe[BRIEFING_CAVE_OFFSET..BRIEFING_CAVE_OFFSET+13]!=[0xcc;13]
        || exe[BRIEFING_DEREF_OFFSET..BRIEFING_DEREF_OFFSET+3]!=[0x8b,0x48,0x44]{
        return Err("Unrecognized Matrox briefing-list patch sites; refusing EXE modification".into());
    }
    exe[BRIEFING_CAVE_OFFSET..BRIEFING_CAVE_OFFSET+13].copy_from_slice(&BRIEFING_CAVE);
    exe[BRIEFING_DEREF_OFFSET..BRIEFING_DEREF_OFFSET+3].copy_from_slice(&BRIEFING_GUARD_JUMP);
    Ok(())
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
    apply_null_guard(&mut exe)?;
    apply_briefing_guard(&mut exe)?;
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
/// Authenticate the original OEM files in private staging and preserve only
/// the exact original EXE as .bak. The original proprietary MSI95 is discarded
/// after ISO verification and the embedded renderer is installed directly.
pub fn prepare_staged(game: &Path, launcher_root: &Path)->Result<(),String>{
    matrox::verify_original(game)?;
    let driver=cleanroom_driver(launcher_root)?;
    let exe_path=game.join("splanetw.exe");
    let original_exe=game.join("splanetw.exe.bak");
    let original_driver=game.join("msi95.dll");
    if fs::symlink_metadata(&original_exe).is_ok() {
        return Err(format!("Matrox EXE backup already exists: {}",original_exe.display()));
    }
    if fs::symlink_metadata(game.join("msi95.dll.bak")).is_ok() {
        return Err("Unexpected msi95.dll.bak inside private install stage".into());
    }
    let exe=patched_exe(&exe_path)?;
    // All changes below affect only the private uncommitted stage.
    fs::rename(&exe_path,&original_exe).map_err(|e|e.to_string())?;
    write_new(&exe_path,&exe)?;
    // The proprietary DLL was hash-checked against the disc manifest before
    // staging. Do not keep any DLL .bak in the installed game.
    fs::remove_file(&original_driver).map_err(|e|e.to_string())?;
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
/// The short-lived '.replacing' file exists only during atomic rollback
/// and is deleted after successful verification. No original DLL backup
/// is retained from the authenticated user's disc.
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

/// Upgrade only exact known previous installer-patched EXEs to the current
/// NULL-guarded and briefing-list-guarded version. Reject arbitrary EXEs. Stage the
/// complete hash-verified new file, use a temporary rollback (not a new
/// permanent .bak), and never touch the separately authenticated original
/// splanetw.exe.bak. Missing/unknown active EXE is an error.
fn ensure_game_compat_exe(game:&Path,mut log:impl FnMut(&str))->Result<(),String>{
    let active=game.join("splanetw.exe");
    let prior=hash_file(&active)?;
    if prior==PATCHED_EXE_SHA{return Ok(());}
    if prior!=PREVIOUS_PATCHED_EXE_SHA && prior!=PREVIOUS_NULL_GUARDED_EXE_SHA{
        return Err(format!("Unsupported Matrox game EXE {} sha256={prior}; refusing patch",
                           active.display()));
    }
    let mut modified=fs::read(&active).map_err(|e|format!("Cannot read Matrox game EXE: {e}"))?;
    if prior==PREVIOUS_PATCHED_EXE_SHA{apply_null_guard(&mut modified)?;}
    apply_briefing_guard(&mut modified)?;
    if digest(&modified)!=PATCHED_EXE_SHA{
        return Err("Guarded Matrox game EXE failed expected SHA-256".into());
    }
    let staged=game.join(".splanetw.exe.installing");
    let rollback=game.join(".splanetw.exe.replacing");
    for path in [&staged,&rollback]{
        if fs::symlink_metadata(path).is_ok(){
            return Err(format!("Interrupted Matrox game EXE upgrade; inspect {}",path.display()));
        }
    }
    if let Err(e)=write_new(&staged,&modified){
        let _=fs::remove_file(&staged);
        return Err(e);
    }
    if hash_file(&staged)?!=PATCHED_EXE_SHA || hash_file(&active)?!=prior{
        let _=fs::remove_file(&staged);
        return Err("Staged Matrox game EXE or previous EXE changed during upgrade".into());
    }
    if let Err(e)=fs::rename(&active,&rollback){
        let _=fs::remove_file(&staged);
        return Err(format!("Cannot stage old Matrox EXE for rollback: {e}"));
    }
    if let Err(e)=fs::rename(&staged,&active){
        let restored=fs::rename(&rollback,&active);
        let _=fs::remove_file(&staged);
        return Err(format!("Cannot publish null-guarded Matrox EXE: {e}; restore={restored:?}"));
    }
    if let Err(e)=hash_file(&active).and_then(|sha|
        if sha==PATCHED_EXE_SHA{Ok(())}else{Err("Unexpected Matrox EXE SHA-256".into())}){
        let restored=fs::remove_file(&active).and_then(|_|fs::rename(&rollback,&active));
        return Err(format!("Matrox EXE upgrade verification failed: {e}; restore={restored:?}"));
    }
    fs::remove_file(&rollback)
        .map_err(|e|format!("Matrox EXE upgraded but rollback cleanup failed: {e}"))?;
    log("MATROX: original splanetw.exe.bak preserved; upgraded exact recognized EXE with null-object and briefing-list guards; no new backup.");
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
    log("MSI95: embedded DLL installed and verified; temporary rollback removed; no permanent DLL backup.");
    Ok(())
}

/// Remove an obsolete OEM driver backup created by older experimental
/// installers, but ONLY when it exactly matches the authenticated original
/// Matrox driver from the CD manifest. Unrecognized user files are preserved.
fn cleanup_legacy_driver_backup(game:&Path,expected:&str,
                                 mut log:impl FnMut(&str))->Result<(),String>{
    let path=game.join("msi95.dll.bak");
    let current=match fs::symlink_metadata(&path){
        Ok(_)=>hash_file(&path)?,
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>return Ok(()),
        Err(e)=>return Err(format!("Cannot inspect legacy DLL backup {}: {e}",path.display())),
    };
    if current!=expected{
        log("MSI95: unrecognized legacy msi95.dll.bak preserved (not safe to remove automatically).");
        return Ok(());
    }
    fs::remove_file(&path)
        .map_err(|e|format!("Cannot delete obsolete OEM DLL backup {}: {e}",path.display()))?;
    log("MSI95: removed authenticated legacy msi95.dll.bak; no DLL backup retained.");
    Ok(())
}

/// The launcher owns game/msi95.dll, even if previous contents are unknown.
/// Verify the original OEM game resources and original EXE backup before
/// replacing it, but never require or create a DLL .bak.
pub fn prepare(root:&Path,mut log:impl FnMut(&str))->Result<PathBuf,String>{
    let game=root.join(GAME_FOLDER);
    matrox::verify_installed_oem(&game)
        .map_err(|e|format!("Cannot prepare Matrox MSI95: original game authentication failed: {e}"))?;
    check_ini(&game.join("splanet.ini"))?;
    ensure_game_compat_exe(&game,|message|log(message))?;
    ensure_owned_driver(&game,expected_driver_hash(),||cleanroom_driver(root),
        |message|log(message),||verify_prepared(root))?;
    cleanup_legacy_driver_backup(&game,matrox::ORIGINAL_MSI95,|message|log(message))?;
    matrox_iso::update_generated_ini(&game)?;
    settings::ensure_defaults(&root.join("ScorchedPlanetFix.ini"),true)?;
    Ok(game)
}

fn configure(command: &mut Command,root:&Path,force_first_log:bool)->Result<(),String> {
    let configured=settings::read_settings(&root.join("ScorchedPlanetFix.ini"))?;
    let controlled=[
        "MATROX_MSI_BORDERLESS","MATROX_MSI_PERSPECTIVE",
        "MATROX_MSI_INDEXED_KEY","MATROX_MSI_SHADOW_ONCE",
        "MATROX_MSI_MODULATE","MATROX_MSI_DITHER","MATROX_MSI_DITHER_GOURAUD",
        "MATROX_MSI_FPS_LIMIT","MATROX_MSI_LOG",
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
    command.env("MATROX_MSI_DITHER_GOURAUD",if configured.dither_gouraud{"1"}else{"0"});
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
        assert_eq!(NULL_GUARD_CAVE_OFFSET,0x93d53);
        assert_eq!(NULL_DEREF_OFFSET,0x93d65);
        assert_eq!(BRIEFING_CAVE_OFFSET,0x84c53);
        assert_eq!(BRIEFING_DEREF_OFFSET,0x84cce);
        assert_eq!(PREVIOUS_NULL_GUARDED_EXE_SHA.len(),64);
        assert_eq!(PREVIOUS_PATCHED_EXE_SHA.len(),64);
        assert_eq!(matrox::manifest_entries().unwrap().len(),66);
        assert_eq!(PATCHED_EXE_SHA.len(),64);
    }
    #[test]
    fn null_guard_changes_only_the_13_byte_cave_and_three_byte_deref(){
        let mut exe=vec![0u8;840_704];
        exe[NULL_GUARD_CAVE_OFFSET..NULL_GUARD_CAVE_OFFSET+13].fill(0xcc);
        exe[NULL_DEREF_OFFSET..NULL_DEREF_OFFSET+3].copy_from_slice(&[0x8b,0x46,0x24]);
        let before=exe.clone();
        apply_null_guard(&mut exe).unwrap();
        assert_eq!(&exe[NULL_GUARD_CAVE_OFFSET..NULL_GUARD_CAVE_OFFSET+13],&NULL_GUARD_CAVE);
        assert_eq!(&exe[NULL_DEREF_OFFSET..NULL_DEREF_OFFSET+3],&NULL_GUARD_JUMP);
        let changed=exe.iter().zip(&before).filter(|(a,b)|a!=b).count();
        assert_eq!(changed,16);
        assert!(apply_null_guard(&mut exe).is_err(),"double patch is refused");
    }
    #[test]
    fn briefing_list_guard_skips_only_misaligned_object_entries(){
        let mut exe=vec![0u8;840_704];
        exe[BRIEFING_CAVE_OFFSET..BRIEFING_CAVE_OFFSET+13].fill(0xcc);
        exe[BRIEFING_DEREF_OFFSET..BRIEFING_DEREF_OFFSET+3]
            .copy_from_slice(&[0x8b,0x48,0x44]);
        let original=exe.clone();
        apply_briefing_guard(&mut exe).unwrap();
        assert_eq!(&exe[BRIEFING_CAVE_OFFSET..BRIEFING_CAVE_OFFSET+13],&BRIEFING_CAVE);
        assert_eq!(&exe[BRIEFING_DEREF_OFFSET..BRIEFING_DEREF_OFFSET+3],&BRIEFING_GUARD_JUMP);
        assert_eq!(exe.iter().zip(&original).filter(|(a,b)|a!=b).count(),16);
        // All object pointers from the game's allocator are DWORD-aligned;
        // an observed element value 2 must be discarded, not dereferenced.
        assert_ne!(2u32 & 3,0);
        assert_eq!(0x01781170u32 & 3,0);
        assert!(apply_briefing_guard(&mut exe).is_err(),"double patch refused");
    }
    #[test]
    fn unknown_game_exe_never_gets_patched_or_backed_up(){
        let root=std::env::temp_dir().join(format!("matrox-nullguard-refuse-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let active=root.join("splanetw.exe");
        fs::write(&active,b"custom game executable").unwrap();
        assert!(ensure_game_compat_exe(&root,|_|{}).is_err());
        assert_eq!(fs::read(&active).unwrap(),b"custom game executable");
        assert!(!root.join(".splanetw.exe.installing").exists());
        assert!(!root.join(".splanetw.exe.replacing").exists());
        assert!(!root.join("splanetw.exe.bak").exists());
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn environment_gets_shared_root_ini_values(){
        let root=std::env::temp_dir().join(format!("matrox-layout-env-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("ScorchedPlanetFix.ini"),
            b"[scorch]\nFPSLimit=120\n[matrox]\nDither=1\nDitherGouraud=1\n").unwrap();
        let mut c=Command::new("unused");
        configure(&mut c,&root,false).unwrap();
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_FPS_LIMIT" &&
            v==Some(std::ffi::OsStr::new("120"))));
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_DITHER" &&
            v==Some(std::ffi::OsStr::new("1"))));
        assert!(c.get_envs().any(|(k,v)|k=="MATROX_MSI_DITHER_GOURAUD" &&
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
    fn authenticated_old_oem_dll_backup_is_removed_but_unknown_is_preserved(){
        let root=std::env::temp_dir().join(format!("matrox-old-dll-backup-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let backup=root.join("msi95.dll.bak");
        let original=b"authentic original DLL in fixture";
        let expected=digest(original);
        fs::write(&backup,original).unwrap();
        cleanup_legacy_driver_backup(&root,&expected,|_|{}).unwrap();
        assert!(!backup.exists());
        fs::write(&backup,b"unrecognized user data").unwrap();
        cleanup_legacy_driver_backup(&root,&expected,|_|{}).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),b"unrecognized user data");
        fs::remove_file(&backup).unwrap();
        cleanup_legacy_driver_backup(&root,&expected,|_|{}).unwrap();
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn missing_game_does_not_create_second_runtime(){
        let root=std::env::temp_dir().join(format!("matrox-missing-{}",std::process::id()));
        assert!(prepare(&root,|_|{}).is_err());
        assert!(!root.join(GAME_FOLDER).exists());
    }
}
