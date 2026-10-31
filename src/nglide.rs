// SPDX-License-Identifier: GPL-3.0-or-later
//! User-installed nGlide integration: NO download and NO bundled proprietary bits.
//! nGlide explicitly supports environment variables overriding global registry
//! settings (Zeus Software). Per-game process environment is safer than registry.
//! https://www.zeus-software.com/forum/viewtopic.php?p=4254
use std::fs::{self,OpenOptions};
use std::io::{Read,Write};
use std::path::{Path,PathBuf};
use std::process::Command;
use sha2::{Digest,Sha256};

pub const OFFICIAL_URL:&str="https://www.zeus-software.com/downloads/nglide";
fn validate_glide_dll(path:&Path)->Result<(),String>{
    let m=fs::symlink_metadata(path).map_err(|e|format!("Missing nGlide glide.dll: {} ({e})",path.display()))?;
    if !m.is_file()||m.file_type().is_symlink()||m.len()<1024||m.len()>16*1024*1024 {
        return Err(format!("glide.dll is not a regular file or has an invalid size: {}",path.display()));
    }
    let mut f=fs::File::open(path).map_err(|e|e.to_string())?;
    let mut header=[0u8;4096];
    let count=f.read(&mut header).map_err(|e|e.to_string())?;
    if count<256 || &header[..2]!=b"MZ" {return Err(format!("glide.dll is not a PE binary: {}",path.display()));}
    let pos=u32::from_le_bytes(header[0x3c..0x40].try_into().unwrap()) as usize;
    // A generic x86 EXE renamed to glide.dll is not a Glide wrapper.
    // Require the PE32 optional header and the IMAGE_FILE_DLL flag as well
    // as the x86 machine type, without needing to execute third-party code.
    if pos.checked_add(26).is_none_or(|p|p>count) ||
       &header[pos..pos+4]!=b"PE\0\0" ||
       u16::from_le_bytes(header[pos+4..pos+6].try_into().unwrap())!=0x14c ||
       u16::from_le_bytes(header[pos+22..pos+24].try_into().unwrap())&0x2000==0 ||
       u16::from_le_bytes(header[pos+24..pos+26].try_into().unwrap())!=0x10b {
        return Err(format!("glide.dll must be a 32-bit Windows x86 DLL: {}",path.display()));
    }
    // Explicitly reject the exact original 1996 Glide hardware driver
    // found on BOTH European CDs. It is not Zeus Software's modern nGlide.
    const ORIGINAL_CD_GLIDE:&str="3e81d822a0185766089948114853ce4dfc4fb5ab76f2fdca5f81ce441f29986e";
    let bytes=fs::read(path).map_err(|e|e.to_string())?;
    if format!("{:x}",Sha256::digest(&bytes))==ORIGINAL_CD_GLIDE{
        return Err("glide.dll is the original CD's obsolete 3dfx hardware driver, not nGlide. Install nGlide 2.10 from Zeus Software.".into());
    }
    Ok(())
}
fn find_source(root:&Path)->Option<PathBuf>{
    // The owner can place a copy of their legitimately installed official
    // nGlide glide.dll beside this tool; no network, no installer execution.
    let local=root.join("glide.dll");
    if validate_glide_dll(&local).is_ok(){return Some(local);}
    // For nGlide already installed by its official setup in this Windows/Wine
    // prefix, copy only its locally installed 32-bit system wrapper.
    for var in ["WINDIR","SystemRoot"] {
        if let Some(system)=std::env::var_os(var){
            let path=PathBuf::from(system).join("SysWOW64").join("glide.dll");
            if validate_glide_dll(&path).is_ok(){return Some(path);}
        }
    }
    None
}
/// Installation helper: copy the *owner's locally installed* glide.dll into
/// staged game directory, without touching the source or overriding an
/// existing wrapper. This is not distribution or remote download.
pub fn copy_installed_glide(root:&Path,game:&Path,mut log:impl FnMut(&str))->Result<bool,String>{
    let output=game.join("glide.dll");
    if output.exists()||output.is_symlink(){
        validate_glide_dll(&output)?;
        log("NGLIDE: existing local 32-bit glide.dll retained");
        return Ok(true);
    }
    let Some(source)=find_source(root) else {
        log(&format!("NGLIDE WARNING: no locally installed 32-bit nGlide wrapper found. Install nGlide yourself from {OFFICIAL_URL} or place your own glide.dll beside ScorchedPlanetFix.exe; --verify/--launch will refuse until game/splanet/glide.dll exists."));
        return Ok(false);
    };
    let mut src=fs::File::open(&source).map_err(|e|e.to_string())?;
    let mut dest=OpenOptions::new().write(true).create_new(true).open(&output)
        .map_err(|e|format!("Unable to copy the user-provided Glide DLL: {e}"))?;
    // We alone created this destination. On any I/O or validation error,
    // remove the incomplete DLL so a later retry can install a good copy.
    let result=(||{
        std::io::copy(&mut src,&mut dest).map_err(|e|e.to_string())?;
        dest.flush().map_err(|e|e.to_string())?;
        dest.sync_all().map_err(|e|e.to_string())?;
        drop(dest);
        validate_glide_dll(&output)
    })();
    if let Err(e)=result{
        let _=fs::remove_file(&output);
        return Err(e);
    }
    log(&format!("NGLIDE: user-installed 32-bit glide.dll copied into game directory from {}",source.display()));
    Ok(true)
}
/// Check a locally supplied splash DLL's architecture and ordinary-file status
/// without loading its code. This does not authenticate a publisher.
fn validate_splash_dll(path:&Path)->Result<(),String>{
    let meta=fs::symlink_metadata(path)
        .map_err(|e|format!("Cannot inspect 3DfxSpl.dll at {}: {e}",path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink() ||
       !(1024..=16*1024*1024).contains(&meta.len()){
        return Err(format!("3DfxSpl.dll is not a regular DLL-sized file: {}",path.display()));
    }
    let mut f=fs::File::open(path).map_err(|e|e.to_string())?;
    let mut header=[0u8;4096];
    let count=f.read(&mut header).map_err(|e|e.to_string())?;
    if count<256 || &header[..2]!=b"MZ"{
        return Err(format!("3DfxSpl.dll lacks a PE header: {}",path.display()));
    }
    let offset=u32::from_le_bytes(header[0x3c..0x40].try_into().unwrap()) as usize;
    if offset.checked_add(26).is_none_or(|p|p>count) ||
       &header[offset..offset+4]!=b"PE\0\0" ||
       u16::from_le_bytes(header[offset+4..offset+6].try_into().unwrap())!=0x14c ||
       u16::from_le_bytes(header[offset+22..offset+24].try_into().unwrap())&0x2000==0 ||
       u16::from_le_bytes(header[offset+24..offset+26].try_into().unwrap())!=0x10b{
        return Err(format!("3DfxSpl.dll must be a 32-bit x86 Windows DLL: {}",path.display()));
    }
    Ok(())
}
/// The Glide 2.11 (glide.dll) splash animation uses 3DfxSpl.dll, not
/// 3DfxSpl2.dll/3DfxSpl3.dll. Explicitly requested logos require that plugin.
/// Only copy a user-installed/local DLL; never download or redistribute it.
pub fn prepare_splash_plugin(game:&Path,mut log:impl FnMut(&str))->Result<(),String>{
    let output=game.join("3DfxSpl.dll");
    if output.exists() || output.is_symlink(){
        validate_splash_dll(&output)?;
        log(&format!("NGLIDE: 3DfxSpl.dll found in game directory: {}",output.display()));
        return Ok(());
    }
    let mut candidates=Vec::new();
    if let Some(parent)=game.parent(){candidates.push(parent.join("3DfxSpl.dll"));}
    for var in ["WINDIR","SystemRoot"]{
        if let Some(windir)=std::env::var_os(var){
            for subfolder in ["SysWOW64","System32"]{
                let candidate=PathBuf::from(&windir).join(subfolder).join("3DfxSpl.dll");
                if !candidates.contains(&candidate){candidates.push(candidate);}
            }
        }
    }
    for source in candidates{
        if !source.exists() && !source.is_symlink(){continue;}
        validate_splash_dll(&source)?;
        // Create only, never clobber an existing file. No global registry edits.
        fs::copy(&source,&output)
            .map_err(|e|format!("Cannot copy local 3DfxSpl.dll from {} to {}: {e}",
                source.display(),output.display()))?;
        if let Err(e)=validate_splash_dll(&output){
            let _=fs::remove_file(&output);
            return Err(e);
        }
        log(&format!("NGLIDE: 3DfxSpl.dll copied from {} into game directory for requested logo",
            source.display()));
        return Ok(());
    }
    Err(format!("3dfx logo requested but 3DfxSpl.dll not found. This Glide 2.11 splash plugin is separate from glide.dll. Install nGlide in the same Windows/Wine prefix or place your own 32-bit 3DfxSpl.dll next to ScorchedPlanetFix.exe. No DLL is bundled or downloaded. Official nGlide: {OFFICIAL_URL}"))
}
pub fn verify_local_glide(game:&Path)->Result<(),String>{
    validate_glide_dll(&game.join("glide.dll")).map_err(|e|
        format!("{e}. nGlide is required in the splanet directory. Download and install it separately: {OFFICIAL_URL}. ScorchedPlanetFix does not redistribute the proprietary nGlide DLL."))
}
#[cfg(windows)]
fn has_vulkan_loader()->bool{
    use std::ffi::c_void;
    #[link(name="kernel32")]
    extern "system"{
        fn LoadLibraryW(file:*const u16)->*mut c_void;
        fn FreeLibrary(handle:*mut c_void)->i32;
    }
    let path:Vec<u16>="vulkan-1.dll".encode_utf16().chain(std::iter::once(0)).collect();
    unsafe{
        let m=LoadLibraryW(path.as_ptr());
        if m.is_null(){false}else{FreeLibrary(m);true}
    }
}
/// Per-game environment overrides global nGlide configurator/registry.
/// Vulkan is requested only when its loader can be opened, not guaranteed
/// to have a working GPU/driver. 60Hz + vsync helps keep old games in check.
pub fn configure_process(command:&mut Command,show_3dfx_logo:bool,mut log:impl FnMut(&str)){
    let backend=if has_vulkan_loader(){"2"}else{"0"};
    for (name,value) in [
        ("NGLIDE_BACKEND",backend), // Vulkan when loader present, otherwise automatic
        ("NGLIDE_RESOLUTION","1"), // By desktop
        ("NGLIDE_ASPECT","1"), // Aspect correction
        ("NGLIDE_REFRESH","2"), // 60 Hz
        ("NGLIDE_VSYNC","1"), // On: minimize tearing / runaway frame rate
        ("NGLIDE_GAMMA","5"), // 1.0
        ("NGLIDE_SPLASH",if show_3dfx_logo{"1"}else{"0"}), // On only with --launch --3dfx-logo
    ]{command.env(name,value);}
    log(&format!("NGLIDE: process-local settings (registry unchanged): backend={} (2=Vulkan loader found; 0=auto), resolution=desktop, aspect=correction, 60Hz, vsync=on, gamma=1.0, splash={}",backend,if show_3dfx_logo{"on"}else{"off"}));
}
#[cfg(test)]
mod tests{
    use super::*;
    #[test] fn splash_logo_is_off_by_default_and_on_only_when_requested(){
        let mut cmd=Command::new("spvoodoo.exe");
        configure_process(&mut cmd,false,|_|{});
        assert_eq!(
            cmd.get_envs().find(|(key,_)|*key==std::ffi::OsStr::new("NGLIDE_SPLASH")).and_then(|(_,value)|value),
            Some(std::ffi::OsStr::new("0"))
        );
        let mut cmd=Command::new("spvoodoo.exe");
        configure_process(&mut cmd,true,|_|{});
        assert_eq!(
            cmd.get_envs().find(|(key,_)|*key==std::ffi::OsStr::new("NGLIDE_SPLASH")).and_then(|(_,value)|value),
            Some(std::ffi::OsStr::new("1"))
        );
    }
    #[test] fn user_supplied_splash_plugin_is_copied_only_on_request(){
        let root=std::env::temp_dir().join(format!("nglide-splash-test-{}",std::process::id()));
        let game=root.join("splanet");
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&game).unwrap();
        let path=root.join("3DfxSpl.dll");
        let mut dll=vec![0u8;2048];
        dll[..2].copy_from_slice(b"MZ");
        dll[0x3c..0x40].copy_from_slice(&0x80u32.to_le_bytes());
        dll[0x80..0x84].copy_from_slice(b"PE\0\0");
        dll[0x84..0x86].copy_from_slice(&0x14cu16.to_le_bytes());
        dll[0x96..0x98].copy_from_slice(&0x2000u16.to_le_bytes());
        dll[0x98..0x9a].copy_from_slice(&0x10bu16.to_le_bytes());
        fs::write(&path,&dll).unwrap();
        assert!(!game.join("3DfxSpl.dll").exists());
        prepare_splash_plugin(&game,|_|{}).unwrap();
        assert_eq!(fs::read(game.join("3DfxSpl.dll")).unwrap(),dll);
        prepare_splash_plugin(&game,|_|{}).unwrap();
        fs::write(game.join("3DfxSpl.dll"),b"tampered").unwrap();
        assert!(prepare_splash_plugin(&game,|_|{}).is_err());
        assert_eq!(fs::read(game.join("3DfxSpl.dll")).unwrap(),b"tampered");
        fs::remove_dir_all(root).unwrap();
    }
    #[test] fn no_network_or_bundled_wrapper_required(){
        assert!(OFFICIAL_URL.starts_with("https://"));
        let root=std::env::temp_dir().join(format!("nglide-test-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        assert!(verify_local_glide(&root).is_err());
        fs::remove_dir_all(root).unwrap();
    }
    #[test] fn require_real_pe32_dll_header(){
        let dir=std::env::temp_dir().join(format!("nglide-dll-header-{}",std::process::id()));
        let _=fs::remove_dir_all(&dir);
        fs::create_dir(&dir).unwrap();
        let path=dir.join("glide.dll");
        let mut bytes=vec![0u8;2048];
        bytes[..2].copy_from_slice(b"MZ");
        bytes[0x3c..0x40].copy_from_slice(&0x80u32.to_le_bytes());
        let pe=0x80;
        bytes[pe..pe+4].copy_from_slice(b"PE\0\0");
        bytes[pe+4..pe+6].copy_from_slice(&0x14cu16.to_le_bytes());
        bytes[pe+22..pe+24].copy_from_slice(&0x2000u16.to_le_bytes());
        bytes[pe+24..pe+26].copy_from_slice(&0x10bu16.to_le_bytes());
        fs::write(&path,&bytes).unwrap();
        assert!(validate_glide_dll(&path).is_ok());
        bytes[pe+22..pe+24].copy_from_slice(&0u16.to_le_bytes());
        fs::write(&path,&bytes).unwrap();
        assert!(validate_glide_dll(&path).is_err(),"renamed EXE must not pass as DLL");
        bytes[pe+22..pe+24].copy_from_slice(&0x2000u16.to_le_bytes());
        bytes[pe+24..pe+26].copy_from_slice(&0x20bu16.to_le_bytes());
        fs::write(&path,&bytes).unwrap();
        assert!(validate_glide_dll(&path).is_err(),"PE32+ must be rejected");
        fs::remove_dir_all(dir).unwrap();
    }
    #[test] fn reject_invalid_or_64_bit_glide(){
        let dir=std::env::temp_dir().join(format!("nglide-pe-test-{}",std::process::id()));
        let _=fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        let file=dir.join("glide.dll");
        fs::write(&file,b"not a DLL").unwrap();
        assert!(validate_glide_dll(&file).is_err());
        fs::remove_dir_all(dir).unwrap();
    }
}
