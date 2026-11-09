// SPDX-License-Identifier: GPL-3.0-or-later
//! GameFrame Glide 2.11 -> OpenGL: the only supported v2 Glide backend.
//! Builds from pinned Apache-2.0 source, embeds the 32-bit DLL into the
//! Windows x64 launcher. A different regular glide.dll is backed up before
//! installation of the embedded GameFrame; unknown files are never discarded.
use sha2::{Digest, Sha256};
use std::fs::{self, OpenOptions};
use std::io::{Read,Write};
use std::path::Path;
use std::process::Command;

#[link(name="user32")]
extern "system" { fn GetSystemMetrics(index:i32)->i32; }

/// Largest centered 4:3 picture fitting inside the unchanged desktop.
fn fit_4_3(width:u32,height:u32)->(u32,u32){
    if (width as u64)*3>(height as u64)*4 {
        (height.saturating_mul(4)/3,height)
    }else{
        (width,width.saturating_mul(3)/4)
    }
}

/// Primary desktop dimensions are read-only and are never applied as a
/// ChangeDisplaySettings request. Wine exposes its configured current mode.
fn desktop()->Result<(u32,u32),String>{
    let (w,h)=unsafe{(GetSystemMetrics(0),GetSystemMetrics(1))};
    if w<640||h<480 {
        return Err(format!("Invalid desktop metrics {w}x{h}; refusing window resize"));
    }
    Ok((w as u32,h as u32))
}

fn vacant(path:&Path)->Result<(),String>{
    match fs::symlink_metadata(path){
        Err(e) if e.kind()==std::io::ErrorKind::NotFound => Ok(()),
        Ok(_)=>Err(format!("GameFrame backup/staging path already exists: {}",path.display())),
        Err(e)=>Err(format!("Cannot inspect {}: {e}",path.display())),
    }
}

/// Hash any *regular* file named glide.dll. The launcher backs up even an
/// unknown or malformed DLL instead of rejecting it; symlinks and directories
/// remain forbidden, and --verify continues to check the exact bundled bytes.
fn regular_file_hash(path:&Path)->Result<String,String>{
    let meta=fs::symlink_metadata(path)
        .map_err(|e|format!("Cannot inspect {}: {e}",path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink(){
        return Err(format!("Refusing to replace non-regular or symlinked Glide DLL: {}",path.display()));
    }
    let mut file=fs::File::open(path)
        .map_err(|e|format!("Cannot open {}: {e}",path.display()))?;
    let mut hasher=Sha256::new();
    let mut buf=[0u8;65536];
    loop{
        let n=file.read(&mut buf).map_err(|e|format!("Cannot read {}: {e}",path.display()))?;
        if n==0 {break;}
        hasher.update(&buf[..n]);
    }
    Ok(format!("{:x}",hasher.finalize()))
}

/// Single managed backup slot: previous regular backups are replaced by the
/// current glide.dll during migration. Never follow a symlink or replace dirs.
fn backup_slot(game:&Path)->Result<(std::path::PathBuf,bool),String>{
    let path=game.join("glide.dll.bak");
    match fs::symlink_metadata(&path){
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>Ok((path,false)),
        Ok(meta) if meta.is_file() && !meta.file_type().is_symlink()=>Ok((path,true)),
        Ok(_)=>Err(format!("Refusing to overwrite a non-regular or symlinked Glide backup: {}",path.display())),
        Err(e)=>Err(format!("Cannot inspect Glide backup {}: {e}",path.display())),
    }
}

/// Stage and verify GameFrame before moving anything. Atomically replace
/// glide.dll.bak with the old active DLL and activate the staged DLL. On
/// activation/verification failure, restore the old active DLL from backup.
fn replace_with_backup(game:&Path,old_hash:&str,new_hash:&str)->Result<(std::path::PathBuf,bool),String>{
    let target=game.join("glide.dll");
    let (backup,replaced_existing_backup)=backup_slot(game)?;
    let stage=game.join(".glide-gameframe-update.tmp");
    vacant(&stage)?;
    let staged=(||->Result<(),String>{
        let mut out=OpenOptions::new().write(true).create_new(true).open(&stage)
            .map_err(|e|format!("Cannot stage GameFrame update: {e}"))?;
        out.write_all(GLIDE_DLL).map_err(|e|format!("Cannot write staged GameFrame: {e}"))?;
        out.sync_all().map_err(|e|format!("Cannot sync staged GameFrame: {e}"))?;
        drop(out);
        if digest(&regular_dll(&stage)?)!=new_hash{
            return Err("Staged GameFrame DLL hash mismatch".into());
        }
        Ok(())
    })();
    if let Err(e)=staged{
        let _=fs::remove_file(&stage);
        return Err(e);
    }
    // Prevent replacing user changes made between initial check and backup.
    if regular_file_hash(&target).as_deref()!=Ok(old_hash){
        let _=fs::remove_file(&stage);
        return Err("Existing glide.dll changed before backup; refusing replacement".into());
    }
    // Recheck the backup slot after staging to reject a newly placed symlink
    // or directory. Windows MoveFileExW(REPLACE_EXISTING) and Unix rename(2)
    // replace a regular destination atomically without delete-then-rename.
    if backup_slot(game).map(|(_,exists)|exists)!=Ok(replaced_existing_backup){
        let _=fs::remove_file(&stage);
        return Err("Glide backup changed before replacement; refusing".into());
    }
    if let Err(e)=fs::rename(&target,&backup){
        let _=fs::remove_file(&stage);
        return Err(format!("Cannot back up existing glide.dll to {}: {e}",backup.display()));
    }
    if let Err(e)=fs::rename(&stage,&target){
        let restored=fs::rename(&backup,&target);
        let _=fs::remove_file(&stage);
        return Err(format!("Cannot activate bundled GameFrame: {e}; restore={restored:?}"));
    }
    if let Err(e)=verify_local_gameframe(game){
        let restored=fs::remove_file(&target).and_then(|_|fs::rename(&backup,&target));
        return Err(format!("Bundled GameFrame validation failed after replacement: {e}; restore={restored:?}"));
    }
    Ok((backup,replaced_existing_backup))
}


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

/// Read-only audit: the active wrapper must exactly match our bundled DLL.
/// An unknown, proprietary or modified DLL is never loaded or silently trusted.
pub fn verify_local_gameframe(game:&Path)->Result<(),String>{
    let expected=validate_embedded()?
        .ok_or("ScorchedPlanetFix v2 requires an embedded GameFrame DLL; this build is missing it")?;
    let path=game.join("glide.dll");
    let actual=digest(&regular_dll(&path)?);
    if actual!=expected {
        return Err(format!(
            "Installed glide.dll does not match bundled GameFrame: {} (SHA-256 {actual}). Run the normal launcher to back up and replace it; --verify is read-only.",
            path.display()));
    }
    Ok(())
}

pub fn is_embedded_gameframe(game:&Path)->Result<bool,String>{
    let Some(expected)=validate_embedded()? else {return Ok(false);};
    Ok(digest(&regular_dll(&game.join("glide.dll"))?)==expected)
}

/// Read-only guard for --install-only. A different *regular* glide.dll is
/// eligible for automatic backup, but reject symlinks and backup/staging
/// conflicts before any game executable/INI migration touches game files.
pub fn preflight_glide(game:&Path)->Result<(),String>{
    let expected=validate_embedded()?
        .ok_or("ScorchedPlanetFix v2 requires embedded GameFrame; no external fallback is supported")?;
    let path=game.join("glide.dll");
    match fs::symlink_metadata(&path){
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>Ok(()),
        Ok(_)=>{
            let actual=regular_file_hash(&path)?;
            if actual==expected {return Ok(());}
            vacant(&game.join(".glide-gameframe-update.tmp"))?;
            let _=backup_slot(game)?;
            Ok(())
        }
        Err(e)=>Err(format!("Cannot inspect Glide DLL: {e}")),
    }
}

/// Install the embedded GameFrame. During normal gameplay the caller first
/// audits all immutable game assets; explicit --install-only and first-time
/// staged installation use their own validation procedures.
/// Any different regular glide.dll (older GameFrame or external) replaces
/// the single glide.dll.bak before installing GameFrame. Non-regular paths
/// and symlinks are never replaced.
pub fn ensure_glide(_root:&Path,game:&Path,mut log:impl FnMut(&str))->Result<bool,String>{
    let expected=validate_embedded()?
        .ok_or("ScorchedPlanetFix v2 requires embedded GameFrame; no external fallback is supported")?;
    preflight_glide(game)?;
    let path=game.join("glide.dll");
    match fs::symlink_metadata(&path){
        Ok(_)=>{
            let actual=regular_file_hash(&path)?;
            if actual==expected{
                log("GAMEFRAME: installed glide.dll already matches bundled GameFrame; no replacement");
                return Ok(true);
            }
            // No obsolete fingerprint allowlist: the same safe replacement
            // applies to every different regular DLL. Its identity is the hash.
            log(&format!("GAMEFRAME: existing glide.dll status=nonmatching old_sha256={actual}; starting protected backup and replacement"));
            let (backup,replaced_existing_backup)=replace_with_backup(game,&actual,&expected)?;
            log(&format!("GAMEFRAME: backup saved original={} backup={} replaced_existing_backup={replaced_existing_backup} old_sha256={actual} status=nonmatching",path.display(),backup.display()));
            log(&format!("GAMEFRAME: installed bundled Glide DLL new_sha256={expected} source={SOURCE_COMMIT}; replacement successful; proceeding to game launch"));
            return Ok(true);
        }
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>{}
        Err(e)=>return Err(format!("Cannot inspect GameFrame DLL: {e}")),
    }
    let mut file=OpenOptions::new().write(true).create_new(true).open(&path)
        .map_err(|e|format!("Refusing to overwrite a Glide DLL: {e}"))?;
    let result=(||{
        file.write_all(GLIDE_DLL).map_err(|e|format!("Cannot write GameFrame: {e}"))?;
        file.flush().map_err(|e|format!("Cannot flush GameFrame: {e}"))?;
        file.sync_all().map_err(|e|format!("Cannot sync GameFrame: {e}"))?;
        drop(file);
        verify_local_gameframe(game)
    })();
    if let Err(e)=result{
        let _=fs::remove_file(&path);
        return Err(e);
    }
    log(&format!("GAMEFRAME: installed bundled 32-bit glide.dll source={SOURCE_COMMIT} sha256={expected}; OpenGL backend, Apache-2.0"));
    Ok(true)
}

/// Native desktop 4:3 presentation and 60 FPS. Never configure nGlide.
pub fn configure_process(command:&mut Command,game:&Path,mut log:impl FnMut(&str))
    ->Result<(),String>{
    verify_local_gameframe(game)?;
    let (dw,dh)=desktop()?;
    let (cw,ch)=fit_4_3(dw,dh);
    command.env("GLIDE_SIZE",format!("{cw}x{ch}"));
    command.env_remove("GLIDE_SCALE_FILE");
    command.env("GLIDE_LFB_FILTER","nearest");
    command.env("SCORCHED_GLIDE_DESKTOP","1");
    command.env("SCORCHED_GLIDE_ADOPT_WINDOW","1");
    command.env("SCORCHED_GLIDE_FPS_60","1");
    // Carry the launcher identity into the 32-bit renderer. Do not hardcode
    // the GameFrame source commit or an old release version as the UI identity.
    command.env("SCORCHED_FIX_SIGNATURE",format!(
        "ScorchedPlanetFix {}-{}",env!("CARGO_PKG_VERSION"),env!("SCORCHED_COMMIT")));
    // Temporary diagnostics: distinguish menu, FMV, option screens and 3D scenes
    // before adding a version overlay that must NOT appear during gameplay.
    command.env("SCORCHED_MENU_PROBE","1");
    if let Some(root)=game.parent(){
        command.env("GLIDE_LOG",root.join("ScorchedPlanetFix.log"));
    }
    let prior=std::env::var("WINEDLLOVERRIDES").unwrap_or_default();
    command.env("WINEDLLOVERRIDES",format!("glide=n;{prior}"));
    log(&format!("GAMEFRAME: backend=OpenGL, API=Glide 2.11, original=640x480, desktop={dw}x{dh} unchanged, 4:3 content={cw}x{ch} centered, one borderless native game HWND, cap=60fps, override=glide=n"));
    Ok(())
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
    fn centered_display_calculations(){
        assert_eq!(fit_4_3(1920,1080),(1440,1080));
        assert_eq!(fit_4_3(2560,1440),(1920,1440));
        assert_eq!(fit_4_3(1280,1024),(1280,960));
        assert_eq!(fit_4_3(1024,768),(1024,768));
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
        preflight_glide(&root).unwrap(); // absent Glide DLL is safe to provision
        assert!(ensure_glide(&root,&root,|_|{}).unwrap());
        preflight_glide(&root).unwrap(); // exact GameFrame DLL is trusted
        let dll=root.join("glide.dll");
        assert_eq!(digest(&fs::read(&dll).unwrap()),expected);
        assert!(is_embedded_gameframe(&root).unwrap());
        verify_local_gameframe(&root).unwrap();
        assert!(ensure_glide(&root,&root,|_|{}).unwrap());
        assert_eq!(digest(&fs::read(&dll).unwrap()),expected);
        let mut cmd=Command::new("test.exe");
        configure_process(&mut cmd,&root,|_|{}).unwrap();
        assert!(cmd.get_envs().any(|(key,val)|key=="GLIDE_LOG" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_GLIDE_FPS_60" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_GLIDE_ADOPT_WINDOW" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="GLIDE_SIZE" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_MENU_PROBE" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_FIX_SIGNATURE"
            && val.is_some_and(|s|s.to_string_lossy().starts_with("ScorchedPlanetFix 2.0.1-"))));
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn unknown_valid_dll_is_backed_up_and_gameframe_installed(){
        let root=fixture();
        let mut foreign=GLIDE_DLL.to_vec();
        let last=foreign.len()-1;
        foreign[last]^=1;
        let file=root.join("glide.dll");
        fs::write(&file,&foreign).unwrap();
        assert!(verify_local_gameframe(&root).is_err()); // --verify remains strict
        preflight_glide(&root).unwrap();
        let mut logs=Vec::new();
        ensure_glide(&root,&root,|entry|logs.push(entry.to_string())).unwrap();
        assert_eq!(fs::read(&root.join("glide.dll.bak")).unwrap(),foreign);
        assert_eq!(fs::read(&file).unwrap(),GLIDE_DLL);
        assert!(logs.iter().any(|s|s.contains("backup saved") && s.contains("old_sha256=")));
        assert!(logs.iter().any(|s|s.contains("replacement successful")));
        verify_local_gameframe(&root).unwrap();
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn existing_backup_is_replaced_without_extra_files(){
        let root=fixture();
        let target=root.join("glide.dll");
        let backup=root.join("glide.dll.bak");
        fs::write(&backup,b"previous backup").unwrap();
        let mut old1=GLIDE_DLL.to_vec();
        *old1.last_mut().unwrap()^=1;
        fs::write(&target,&old1).unwrap();
        let mut logs=Vec::new();
        ensure_glide(&root,&root,|msg|logs.push(msg.to_string())).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),old1);
        assert_eq!(fs::read(&target).unwrap(),GLIDE_DLL);
        assert!(logs.iter().any(|s|s.contains("replaced_existing_backup=true")));
        let mut old2=GLIDE_DLL.to_vec();
        *old2.last_mut().unwrap()^=2;
        fs::write(&target,&old2).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),old2);
        assert_eq!(fs::read(&target).unwrap(),GLIDE_DLL);
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),old2);
        let names=fs::read_dir(&root).unwrap()
            .map(|entry|entry.unwrap().file_name().to_string_lossy().into_owned())
            .collect::<Vec<_>>();
        assert_eq!(names.len(),2,"only active DLL and backup expected: {names:?}");
        assert!(names.contains(&"glide.dll".to_string()));
        assert!(names.contains(&"glide.dll.bak".to_string()));
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn directory_as_backup_blocks_replacement(){
        let root=fixture();
        let target=root.join("glide.dll");
        let backup=root.join("glide.dll.bak");
        fs::write(&target,b"foreign DLL").unwrap();
        fs::create_dir(&backup).unwrap();
        assert!(preflight_glide(&root).is_err());
        assert!(ensure_glide(&root,&root,|_|{}).is_err());
        assert_eq!(fs::read(&target).unwrap(),b"foreign DLL");
        assert!(backup.is_dir());
        assert!(!root.join(".glide-gameframe-update.tmp").exists());
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn invalid_regular_glide_is_backed_up_without_data_loss(){
        let root=fixture();
        let file=root.join("glide.dll");
        fs::write(&file,b"not a DLL").unwrap();
        preflight_glide(&root).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&root.join("glide.dll.bak")).unwrap(),b"not a DLL");
        verify_local_gameframe(&root).unwrap();
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn non_regular_glide_directory_is_never_modified(){
        let root=fixture();
        let invalid=root.join("glide.dll");
        fs::create_dir(&invalid).unwrap();
        assert!(preflight_glide(&root).is_err());
        assert!(ensure_glide(&root,&root,|_|{}).is_err());
        assert!(invalid.is_dir());
        assert!(!root.join("glide.dll.bak").exists());
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn occupied_staging_path_aborts_without_touching_original(){
        let root=fixture();
        let file=root.join("glide.dll");
        fs::write(&file,b"old glide").unwrap();
        fs::write(root.join(".glide-gameframe-update.tmp"),b"reserved").unwrap();
        assert!(preflight_glide(&root).is_err());
        assert!(ensure_glide(&root,&root,|_|{}).is_err());
        assert_eq!(fs::read(&file).unwrap(),b"old glide");
        assert!(!root.join("glide.dll.bak").exists());
        fs::remove_dir_all(&root).unwrap();
    }
}
