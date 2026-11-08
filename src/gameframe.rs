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

// Fingerprints distinguish previous experimental GameFrame wrappers from
// unknown external DLLs in diagnostics. Both are now backed up before replacement.
const PREVIOUS_GAMEFRAME_HASHES:&[&str]=&[
    // Exact SHA-256 values for bundled experimental DLLs recovered from
    // the maintainer's 2026-11-08 Wine log (installed by ScorchedPlanetFix).
    // Keep prior identities for accurate logs and regression coverage.
    "739513dd8ff764f7ff5a88c76f1ac4cc6deb5632eb48f30eed578d31a95fbc9b", // Pre-overlay GameFrame
    "95869c915399651dd192bb8660315869fd4c293c9eef218c2f627415559e3d8d", // Working overlay GameFrame
    "2682b5995994d843d376b8f17115c3b011ca61a3d9e4318d54e9f06040c0747e", // Maintainer-validated former experimental GameFrame; backup before update
    "95f1af3a7f5fc0aa9695b5c8620ac0e24385535d3700259139a2082bcfac219f", // Maintainer-validated former experimental GameFrame; backup before update
    // Exact GameFrame DLL from the 2.0.0-4c1c6d59 Wine acceptance log.
    // This is NOT an arbitrary external wrapper or an unverified DLL.
    // The experimental overlay probe changes GameFrame's bytes, so a safe
    // fingerprinted upgrade keeps the original in a .bak file.
    "cdc88180416d26bd831b94bc271542b1d8e32d1eb86671d1f487ba67a42030bc",
    "1150aebea617cbbd568b92be7f7b1abd3820e510b45c1a24df0a5508960868f7",
    "7b82c9831c3167a0d8ac446fc30ab973d35e9c49e566508ebd5280f3deb8e5a8",
    "37d5c107c65c3fcb2901ba1f4b073aa4dddd13ec016132f63b2fbf545717344e",
    "302dbb2b0c9dbaca0555d6193e2d1629a0254ea93e7acee6687dae2db3181eb1",
];

fn original_gameframe(hash:&str)->bool {
    PREVIOUS_GAMEFRAME_HASHES.contains(&hash)
}

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

/// Prefer glide.dll.bak. Existing backups are never overwritten: each
/// subsequent replacement gets a hash-suffixed or numbered .bak filename.
fn available_backup(game:&Path,old_hash:&str)->Result<std::path::PathBuf,String>{
    let hash=&old_hash[..16];
    for index in 0..=1000 {
        let name=match index {
            0=>"glide.dll.bak".to_string(),
            1=>format!("glide.dll.{hash}.bak"),
            _=>format!("glide.dll.{hash}.{}.bak",index-1),
        };
        let path=game.join(name);
        match fs::symlink_metadata(&path){
            Err(e) if e.kind()==std::io::ErrorKind::NotFound => return Ok(path),
            Ok(_) => continue,
            Err(e) => return Err(format!("Cannot inspect backup {}: {e}",path.display())),
        }
    }
    Err("Too many existing Glide backups; no safe backup filename available".into())
}

/// Stage and verify the embedded 32-bit DLL before moving anything, then
/// rename the original to an unused .bak and activate the staged DLL.
/// Restore the original on any activation/verification failure.
fn replace_with_backup(game:&Path,old_hash:&str,new_hash:&str)->Result<std::path::PathBuf,String>{
    let target=game.join("glide.dll");
    let backup=available_backup(game,old_hash)?;
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
    Ok(backup)
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
            let _=available_backup(game,&actual)?;
            Ok(())
        }
        Err(e)=>Err(format!("Cannot inspect Glide DLL: {e}")),
    }
}

/// After a complete immutable-game-file audit, install the embedded GameFrame.
/// Any prior regular glide.dll (older GameFrame, foreign, or unrecognized) is
/// moved to a unique .bak first. Symlinks and directories are never replaced.
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
            let kind=if original_gameframe(&actual){"older-GameFrame"}else{"unknown-or-external"};
            log(&format!("GAMEFRAME: existing glide.dll status={kind} old_sha256={actual}; starting protected backup and replacement"));
            let backup=replace_with_backup(game,&actual,&expected)?;
            log(&format!("GAMEFRAME: backup saved original={} backup={} old_sha256={actual} status={kind}",path.display(),backup.display()));
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
    fn legacy_gameframe_hashes_are_exact_only(){
        assert!(original_gameframe("1150aebea617cbbd568b92be7f7b1abd3820e510b45c1a24df0a5508960868f7"));
        assert!(original_gameframe("cdc88180416d26bd831b94bc271542b1d8e32d1eb86671d1f487ba67a42030bc"));
        assert!(original_gameframe("739513dd8ff764f7ff5a88c76f1ac4cc6deb5632eb48f30eed578d31a95fbc9b"));
        assert!(original_gameframe("95869c915399651dd192bb8660315869fd4c293c9eef218c2f627415559e3d8d"));
        assert!(!original_gameframe(&"a".repeat(64)));
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
            && val.is_some_and(|s|s.to_string_lossy().starts_with("ScorchedPlanetFix 2.0.0-"))));
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
    fn backup_collision_never_overwrites_existing_backup(){
        let root=fixture();
        let mut foreign=GLIDE_DLL.to_vec();
        *foreign.last_mut().unwrap()^=1;
        let old_hash=digest(&foreign);
        let original_backup=root.join("glide.dll.bak");
        fs::write(&original_backup,b"precious old backup").unwrap();
        fs::write(root.join("glide.dll"),&foreign).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        assert_eq!(fs::read(&original_backup).unwrap(),b"precious old backup");
        let second=root.join(format!("glide.dll.{}.bak",&old_hash[..16]));
        assert_eq!(fs::read(&second).unwrap(),foreign);
        fs::write(root.join("glide.dll"),&foreign).unwrap();
        ensure_glide(&root,&root,|_|{}).unwrap();
        let third=root.join(format!("glide.dll.{}.1.bak",&old_hash[..16]));
        assert_eq!(fs::read(&third).unwrap(),foreign);
        assert_eq!(fs::read(root.join("glide.dll")).unwrap(),GLIDE_DLL);
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
