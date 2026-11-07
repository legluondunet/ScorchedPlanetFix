// SPDX-License-Identifier: GPL-3.0-or-later
//! GameFrame Glide 2.11 -> OpenGL: the only supported v2 Glide backend.
//! Builds from pinned Apache-2.0 source, embeds the 32-bit DLL into the
//! Windows x64 launcher. Unknown external Glide DLLs are rejected without
//! modification; known GameFrame development builds are upgraded with backups.
use sha2::{Digest, Sha256};
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::Path;
use std::process::Command;

#[link(name="user32")]
extern "system" { fn GetSystemMetrics(index:i32)->i32; }

// DLLs shipped only in earlier ScorchedPlanetFix GameFrame CI experiments.
// The exact fingerprints distinguish our previous free wrapper from a user's
// proprietary or unrelated glide.dll; those are NEVER replaced.
const PREVIOUS_GAMEFRAME_HASHES:&[&str]=&[
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

/// Update only a cryptographically recognized former experimental GameFrame
/// binary. Preserve its original bytes in a hash-named backup; never touch
/// unknown or proprietary DLLs. Caller audits all game data before invoking.
fn upgrade_known_gameframe(game:&Path,old_hash:&str,new_hash:&str)->Result<(),String>{
    if !original_gameframe(old_hash) {return Err("Unknown Glide DLL cannot be upgraded".into());}
    let target=game.join("glide.dll");
    let backup=game.join(format!("glide.dll.gameframe-{}.bak",&old_hash[..16]));
    let stage=game.join(".glide-gameframe-update.tmp");
    vacant(&backup)?;
    vacant(&stage)?;
    let mut out=OpenOptions::new().write(true).create_new(true).open(&stage)
        .map_err(|e|format!("Cannot stage GameFrame upgrade: {e}"))?;
    let staged=(||{
        out.write_all(GLIDE_DLL).map_err(|e|e.to_string())?;
        out.sync_all().map_err(|e|e.to_string())?;
        drop(out);
        if digest(&regular_dll(&stage)?)!=new_hash {
            return Err("GameFrame upgrade staged DLL hash mismatch".into());
        }
        Ok(())
    })();
    if let Err(e)=staged {
        let _=fs::remove_file(&stage);
        return Err(e);
    }
    // Exact last-minute hash check prevents replacing user modifications.
    if digest(&regular_dll(&target)?)!=old_hash {
        let _=fs::remove_file(&stage);
        return Err("Original GameFrame DLL changed during upgrade; refusing".into());
    }
    if let Err(e)=fs::rename(&target,&backup){
        let _=fs::remove_file(&stage);
        return Err(format!("Cannot save previous GameFrame DLL: {e}"));
    }
    if let Err(e)=fs::rename(&stage,&target){
        let restore=fs::rename(&backup,&target);
        let _=fs::remove_file(&stage);
        return Err(format!("Cannot activate new GameFrame DLL: {e}; restore={restore:?}"));
    }
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
            "Unsupported external Glide DLL: {} (SHA-256 {actual}). v2 supports GameFrame only. Move this file aside manually and launch again; it will never be overwritten.",
            path.display()));
    }
    Ok(())
}

pub fn is_embedded_gameframe(game:&Path)->Result<bool,String>{
    let Some(expected)=validate_embedded()? else {return Ok(false);};
    Ok(digest(&regular_dll(&game.join("glide.dll"))?)==expected)
}

/// Read-only guard before an explicit --install-only migration can modify
/// the original game executable or INI. No symlink/external DLL is changed.
pub fn preflight_glide(game:&Path)->Result<(),String>{
    let expected=validate_embedded()?
        .ok_or("ScorchedPlanetFix v2 requires embedded GameFrame; no external fallback is supported")?;
    let path=game.join("glide.dll");
    match fs::symlink_metadata(&path){
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>Ok(()),
        Ok(_)=>{
            let actual=digest(&regular_dll(&path)?);
            if actual==expected || original_gameframe(&actual) {return Ok(());}
            Err(format!("Unsupported glide.dll at {} (SHA-256 {actual}). ScorchedPlanetFix v2 supports bundled GameFrame only. Move this DLL aside manually. No game data was modified.",path.display()))
        }
        Err(e)=>Err(format!("Cannot inspect Glide DLL: {e}")),
    }
}

/// Only bundled GameFrame is allowed. Known older GameFrame experimental
/// hashes can upgrade with backup; arbitrary DLLs (including nGlide) are
/// preserved and *rejected*. Caller audits game assets before installing.
pub fn ensure_glide(_root:&Path,game:&Path,mut log:impl FnMut(&str))->Result<bool,String>{
    let expected=validate_embedded()?
        .ok_or("ScorchedPlanetFix v2 requires embedded GameFrame; no external fallback is supported")?;
    preflight_glide(game)?;
    let path=game.join("glide.dll");
    match fs::symlink_metadata(&path){
        Ok(_)=>{
            let actual=digest(&regular_dll(&path)?);
            if actual==expected {
                log("GAMEFRAME: locally installed DLL hash matches bundled GameFrame");
                return Ok(true);
            }
            if original_gameframe(&actual){
                upgrade_known_gameframe(game,&actual,&expected)?;
                log(&format!("GAMEFRAME: upgraded known experimental DLL old_sha256={actual} new_sha256={expected}; backup preserved"));
                return Ok(true);
            }
            return Err(format!("Unsupported glide.dll at {} (SHA-256 {actual}). ScorchedPlanetFix v2 supports bundled GameFrame only. Move the external DLL aside manually; no user file was modified.",path.display()));
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
    if let Err(e)=result {
        // This path was created exclusively by this invocation.
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
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn unknown_but_valid_pe_is_never_accepted_or_overwritten(){
        let root=fixture();
        let mut foreign=GLIDE_DLL.to_vec();
        let last=foreign.len()-1;
        foreign[last]^=1;
        let file=root.join("glide.dll");
        fs::write(&file,&foreign).unwrap();
        assert!(verify_local_gameframe(&root).unwrap_err().contains("supports GameFrame only"));
        assert!(preflight_glide(&root).unwrap_err().contains("No game data was modified"));
        assert!(ensure_glide(&root,&root,|_|{}).unwrap_err().contains("No game data was modified"));
        assert_eq!(fs::read(&file).unwrap(),foreign);
        fs::remove_dir_all(&root).unwrap();
    }

    #[cfg(gameframe_embedded)]
    #[test]
    fn refuses_to_replace_existing_invalid_user_glide(){
        let root=fixture();
        let file=root.join("glide.dll");
        fs::write(&file,b"not a DLL").unwrap();
        assert!(ensure_glide(&root,&root,|_|{}).is_err());
        assert_eq!(fs::read(&file).unwrap(),b"not a DLL");
        fs::remove_dir_all(&root).unwrap();
    }
}
