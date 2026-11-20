// SPDX-License-Identifier: GPL-3.0-or-later
//! GameFrame Glide 2.11 -> OpenGL: the only supported v2 Glide backend.
//! Builds from pinned Apache-2.0 source, embeds the 32-bit DLL into the
//! Windows x64 launcher. A different regular glide.dll is replaced with a
//! temporary rollback; no permanent backup of installer-supplied DLLs.
use crate::settings;
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
        Ok(_)=>Err(format!("GameFrame temporary staging/rollback path already exists: {}",path.display())),
        Err(e)=>Err(format!("Cannot inspect {}: {e}",path.display())),
    }
}

/// Hash any regular glide.dll: even an unknown or malformed DLL is replaceable
/// without permanent backup. Symlinks/directories are not touched.
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

/// A previous glide.dll lives in .glide-gameframe-rollback.tmp only
/// during replacement, and is restored on failure. A pre-existing
/// glide.dll.bak is user data: this function never alters it.
fn replace_with_rollback(game:&Path,old_hash:&str,new_hash:&str,
                         mut verify_after:impl FnMut(&Path)->Result<(),String>)->Result<(),String>{
    let target=game.join("glide.dll");
    let stage=game.join(".glide-gameframe-update.tmp");
    let rollback=game.join(".glide-gameframe-rollback.tmp");
    vacant(&stage)?;
    vacant(&rollback)?;
    let staged=(||->Result<(),String>{
        let mut out=OpenOptions::new().write(true).create_new(true).open(&stage)
            .map_err(|e|format!("Cannot stage GameFrame update: {e}"))?;
        out.write_all(GLIDE_DLL).map_err(|e|format!("Cannot write staged GameFrame: {e}"))?;
        out.sync_all().map_err(|e|format!("Cannot sync staged GameFrame: {e}"))?;
        drop(out);
        if digest(&regular_dll(&stage)?)!=new_hash {
            return Err("Staged GameFrame DLL hash mismatch".into());
        }
        Ok(())
    })();
    if let Err(e)=staged{
        let _=fs::remove_file(&stage);
        return Err(e);
    }
    if regular_file_hash(&target).as_deref()!=Ok(old_hash){
        let _=fs::remove_file(&stage);
        return Err("Existing glide.dll changed during update; refusing replacement".into());
    }
    if let Err(e)=fs::rename(&target,&rollback){
        let _=fs::remove_file(&stage);
        return Err(format!("Cannot stage previous Glide DLL for temporary rollback: {e}"));
    }
    if let Err(e)=fs::rename(&stage,&target){
        let restored=fs::rename(&rollback,&target);
        let _=fs::remove_file(&stage);
        return Err(format!("Cannot activate bundled GameFrame: {e}; restore={restored:?}"));
    }
    if let Err(e)=verify_after(game){
        let restored=fs::remove_file(&target).and_then(|_|fs::rename(&rollback,&target));
        return Err(format!("Bundled GameFrame validation failed after replacement: {e}; restore={restored:?}"));
    }
    fs::remove_file(&rollback)
        .map_err(|e|format!("GameFrame installed but temporary rollback cleanup failed ({}): {e}",rollback.display()))?;
    Ok(())
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
            "Installed glide.dll does not match bundled GameFrame: {} (SHA-256 {actual}). Run the normal launcher to replace it; --verify is read-only.",
            path.display()));
    }
    Ok(())
}

/// Read-only guard for --install-only. Different regular DLLs may be
/// replaced without permanent backup; reject occupied transaction paths,
/// symlinks and directories before changing game files.
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
            vacant(&game.join(".glide-gameframe-rollback.tmp"))?;
            Ok(())
        }
        Err(e)=>Err(format!("Cannot inspect Glide DLL: {e}")),
    }
}

/// Install the embedded GameFrame. During normal gameplay the caller first
/// audits all immutable game assets; explicit --install-only and first-time
/// staged installation use their own validation procedures.
/// Any different regular glide.dll (older GameFrame or external) is replaced
/// using an ephemeral rollback, never a permanent .bak. Existing glide.dll.bak
/// files from earlier installations remain untouched. Non-regular paths
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
            log(&format!("GAMEFRAME: replacing old glide.dll sha256={actual} with bundled sha256={expected}; temporary rollback only"));
            replace_with_rollback(game,&actual,&expected,verify_local_gameframe)?;
            log(&format!("GAMEFRAME: bundled glide.dll installed sha256={expected} source={SOURCE_COMMIT}; rollback removed, existing glide.dll.bak untouched"));
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

/// Native desktop 4:3 presentation and shared INI FPS limit. Never configure nGlide.
/// Strict launch path: require the installed renderer to match bundled GameFrame.
pub fn configure_process(command:&mut Command,game:&Path,session_id:&str,force_first_log:bool,log:impl FnMut(&str))
    ->Result<(),String>{
    configure_process_impl(command,game,session_id,true,force_first_log,log)
}

/// Direct-launch opt-out: configure environment without reading/verifying DLL files.
/// Never install, back up, repair, patch or replace a game/DLL file.
pub fn configure_process_unchecked(command:&mut Command,game:&Path,session_id:&str,log:impl FnMut(&str))
    ->Result<(),String>{
    configure_process_impl(command,game,session_id,false,false,log)
}

fn configure_process_impl(command:&mut Command,game:&Path,session_id:&str,check_renderer:bool,force_first_log:bool,mut log:impl FnMut(&str))
    ->Result<(),String>{
    if check_renderer { verify_local_gameframe(game)?; }
    let (dw,dh)=desktop()?;
    let (cw,ch)=fit_4_3(dw,dh);
    command.env("GLIDE_SIZE",format!("{cw}x{ch}"));
    command.env_remove("GLIDE_SCALE_FILE");
    command.env("GLIDE_LFB_FILTER","nearest");
    command.env("SCORCHED_GLIDE_DESKTOP","1");
    command.env("SCORCHED_GLIDE_ADOPT_WINDOW","1");
    let fixer_ini=game.parent().ok_or("GameFrame game directory has no parent")?
        .join("ScorchedPlanetFix.ini");
    let configured=settings::read_settings(&fixer_ini)?;
    command.env_remove("SCORCHED_GLIDE_FPS_60");
    command.env("SCORCHED_GLIDE_FPS_LIMIT",configured.fps_limit.to_string());
    // Carry the launcher identity into the 32-bit renderer. Do not hardcode
    // the GameFrame source commit or an old release version as the UI identity.
    command.env("SCORCHED_FIX_SIGNATURE",format!(
        "ScorchedPlanetFix {}-{}",env!("CARGO_PKG_VERSION"),env!("SCORCHED_COMMIT")));
    command.env("SCORCHED_FIX_SESSION_ID",session_id);
    // The pinned Scorched-only GameFrame shim checks this flag before ANY
    // file or stderr log write. No renderer log for the normal Log=0 mode.
    // First successful post-install game session is recorded regardless of
    // the user's default Log=0. The root INI is never rewritten.
    let write_log=configured.log || force_first_log;
    command.env("SCORCHED_FIX_LOG",if write_log{"1"}else{"0"});
    if write_log {
        command.env("SCORCHED_MENU_PROBE","1");
        if let Some(root)=game.parent(){
            command.env("GLIDE_LOG",root.join("ScorchedPlanetFix.log"));
        }
    }else{
        command.env_remove("SCORCHED_MENU_PROBE");
        command.env_remove("GLIDE_LOG");
    }
    let prior=std::env::var("WINEDLLOVERRIDES").unwrap_or_default();
    command.env("WINEDLLOVERRIDES",format!("glide=n;{prior}"));
    log(&format!("GAMEFRAME: backend=OpenGL, API=Glide 2.11, original=640x480, desktop={dw}x{dh} unchanged, 4:3 content={cw}x{ch} centered, one borderless native game HWND, cap={}fps (0=unlimited), override=glide=n", configured.fps_limit));
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
    fn unchecked_launch_does_not_require_or_install_gameframe(){
        let root=fixture();
        let game=root.join("splanet");
        let mut command=Command::new(game.join("spvoodoo.exe"));
        configure_process_unchecked(&mut command,&game,"ci-unchecked-42",|_|{}).unwrap();
        assert!(command.get_envs().any(|(name,value)|name=="GLIDE_SIZE" && value.is_some()));
        assert!(!command.get_envs().any(|(name,value)|name=="GLIDE_LOG" && value.is_some()));
        assert!(command.get_envs().any(|(name,value)|name=="SCORCHED_FIX_LOG"
            && value==Some(std::ffi::OsStr::new("0"))));
        assert!(command.get_envs().any(|(name,value)|name=="SCORCHED_FIX_SESSION_ID"
            && value==Some(std::ffi::OsStr::new("ci-unchecked-42"))));
        assert!(!game.exists(),"unchecked mode must not create game directory");
        fs::remove_dir_all(&root).unwrap();
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
        let base=fixture();
        let root=base.join("game");
        fs::create_dir_all(&root).unwrap();
        let expected=validate_embedded().unwrap().unwrap();
        preflight_glide(&root).unwrap(); // absent Glide DLL is safe to provision
        assert!(ensure_glide(&root,&root,|_|{}).unwrap());
        preflight_glide(&root).unwrap(); // exact GameFrame DLL is trusted
        let dll=root.join("glide.dll");
        assert_eq!(digest(&fs::read(&dll).unwrap()),expected);
        verify_local_gameframe(&root).unwrap();
        assert!(ensure_glide(&root,&root,|_|{}).unwrap());
        assert_eq!(digest(&fs::read(&dll).unwrap()),expected);
        let mut cmd=Command::new("test.exe");
        configure_process(&mut cmd,&root,"123456-42",false,|_|{}).unwrap();
        let expected_log=root.parent().unwrap().join("ScorchedPlanetFix.log");
        assert!(!cmd.get_envs().any(|(key,val)|key=="GLIDE_LOG" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_FIX_SESSION_ID"
            && val==Some(std::ffi::OsStr::new("123456-42"))));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_GLIDE_FPS_LIMIT" && val==Some(std::ffi::OsStr::new("60"))));
        fs::write(base.join("ScorchedPlanetFix.ini"),b"[Graphics]\nFPSLimit=120\n[Debug]\nLog=1\n").unwrap();
        configure_process(&mut cmd,&root,"123456-42",false,|_|{}).unwrap();
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_GLIDE_FPS_LIMIT"
            && val==Some(std::ffi::OsStr::new("120"))));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_GLIDE_ADOPT_WINDOW" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="GLIDE_SIZE" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_MENU_PROBE" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="GLIDE_LOG" && val==Some(expected_log.as_os_str())));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_FIX_LOG"
            && val==Some(std::ffi::OsStr::new("1"))));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_FIX_SIGNATURE"
            && val.is_some_and(|s|s.to_string_lossy().starts_with(concat!("ScorchedPlanetFix ", env!("CARGO_PKG_VERSION"), "-")))));
        // First verified launch overrides default Log=0 without changing it.
        fs::write(base.join("ScorchedPlanetFix.ini"),b"[Debug]\nLog=0\n").unwrap();
        configure_process(&mut cmd,&root,"first-play",true,|_|{}).unwrap();
        assert!(cmd.get_envs().any(|(key,val)|key=="GLIDE_LOG" && val==Some(expected_log.as_os_str())));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_FIX_LOG"
            && val==Some(std::ffi::OsStr::new("1"))));
        configure_process(&mut cmd,&root,"later-play",false,|_|{}).unwrap();
        assert!(!cmd.get_envs().any(|(key,val)|key=="GLIDE_LOG" && val.is_some()));
        assert!(cmd.get_envs().any(|(key,val)|key=="SCORCHED_FIX_LOG"
            && val==Some(std::ffi::OsStr::new("0"))));
        fs::remove_dir_all(&base).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn unknown_glide_is_replaced_without_permanent_backup(){
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
        assert_eq!(fs::read(&file).unwrap(),GLIDE_DLL);
        assert!(!root.join("glide.dll.bak").exists());
        assert!(!root.join(".glide-gameframe-rollback.tmp").exists());
        assert!(!root.join(".glide-gameframe-update.tmp").exists());
        assert!(logs.iter().any(|s|s.contains("temporary rollback only")));
        verify_local_gameframe(&root).unwrap();
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn old_backup_is_never_changed_by_multiple_gameframe_updates(){
        let root=fixture();
        let target=root.join("glide.dll");
        let backup=root.join("glide.dll.bak");
        fs::write(&backup,b"user's legacy backup retained").unwrap();
        let mut old1=GLIDE_DLL.to_vec();
        *old1.last_mut().unwrap()^=1;
        fs::write(&target,&old1).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),b"user's legacy backup retained");
        assert_eq!(fs::read(&target).unwrap(),GLIDE_DLL);
        let mut old2=GLIDE_DLL.to_vec();
        *old2.last_mut().unwrap()^=2;
        fs::write(&target,&old2).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),b"user's legacy backup retained");
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&backup).unwrap(),b"user's legacy backup retained");
        assert_eq!(fs::read(&target).unwrap(),GLIDE_DLL);
        assert!(!root.join(".glide-gameframe-rollback.tmp").exists());
        assert!(!root.join(".glide-gameframe-update.tmp").exists());
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn legacy_backup_directory_does_not_block_a_new_driver(){
        let root=fixture();
        let target=root.join("glide.dll");
        let backup=root.join("glide.dll.bak");
        fs::write(&target,b"foreign DLL").unwrap();
        fs::create_dir(&backup).unwrap();
        preflight_glide(&root).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&target).unwrap(),GLIDE_DLL);
        assert!(backup.is_dir(),"never modify an old user-created backup path");
        assert!(!root.join(".glide-gameframe-rollback.tmp").exists());
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn invalid_regular_glide_is_replaced_without_persistent_backup(){
        let root=fixture();
        let file=root.join("glide.dll");
        fs::write(&file,b"not a DLL").unwrap();
        preflight_glide(&root).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert!(!root.join("glide.dll.bak").exists());
        assert!(!root.join(".glide-gameframe-rollback.tmp").exists());
        verify_local_gameframe(&root).unwrap();
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn failed_post_install_verification_restores_earlier_dll(){
        let root=fixture();
        let file=root.join("glide.dll");
        let original=b"external Glide DLL used only for rollback";
        fs::write(&file,original).unwrap();
        let error=replace_with_rollback(&root,&digest(original),
            &validate_embedded().unwrap().unwrap(),
            |_|Err("intentional post-install rejection".into())).unwrap_err();
        assert!(error.contains("intentional post-install rejection"));
        assert_eq!(fs::read(&file).unwrap(),original);
        assert!(!root.join(".glide-gameframe-rollback.tmp").exists());
        assert!(!root.join(".glide-gameframe-update.tmp").exists());
        assert!(!root.join("glide.dll.bak").exists());
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

    #[cfg(gameframe_embedded)]
    #[test]
    fn occupied_rollback_path_aborts_without_touching_original(){
        let root=fixture();
        let file=root.join("glide.dll");
        fs::write(&file,b"old glide").unwrap();
        fs::write(root.join(".glide-gameframe-rollback.tmp"),b"interrupted update").unwrap();
        assert!(preflight_glide(&root).is_err());
        assert!(ensure_glide(&root,&root,|_|{}).is_err());
        assert_eq!(fs::read(&file).unwrap(),b"old glide");
        assert_eq!(fs::read(root.join(".glide-gameframe-rollback.tmp")).unwrap(),b"interrupted update");
        assert!(!root.join("glide.dll.bak").exists());
        fs::remove_dir_all(&root).unwrap();
    }
}
