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
    let m=fs::symlink_metadata(path).map_err(|e|format!("nGlide glide.dll absent : {} ({e})",path.display()))?;
    if !m.is_file()||m.file_type().is_symlink()||m.len()<1024||m.len()>16*1024*1024 {
        return Err(format!("glide.dll non régulier ou taille incorrecte : {}",path.display()));
    }
    let mut f=fs::File::open(path).map_err(|e|e.to_string())?;
    let mut header=[0u8;4096];
    let count=f.read(&mut header).map_err(|e|e.to_string())?;
    if count<256 || &header[..2]!=b"MZ" {return Err(format!("glide.dll n'est pas un exécutable PE : {}",path.display()));}
    let pos=u32::from_le_bytes(header[0x3c..0x40].try_into().unwrap()) as usize;
    if pos.checked_add(6).is_none_or(|p|p>count) ||
       &header[pos..pos+4]!=b"PE\0\0" ||
       u16::from_le_bytes(header[pos+4..pos+6].try_into().unwrap())!=0x14c {
        return Err(format!("glide.dll doit être une DLL Windows x86 (32 bits) : {}",path.display()));
    }
    // Explicitly reject the exact original 1996 Glide hardware driver
    // found on BOTH European CDs. It is not Zeus Software's modern nGlide.
    const ORIGINAL_CD_GLIDE:&str="3e81d822a0185766089948114853ce4dfc4fb5ab76f2fdca5f81ce441f29986e";
    let bytes=fs::read(path).map_err(|e|e.to_string())?;
    if format!("{:x}",Sha256::digest(&bytes))==ORIGINAL_CD_GLIDE{
        return Err("glide.dll est le pilote matériel 3dfx original du CD, et non nGlide. Installer nGlide 2.10 depuis Zeus Software.".into());
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
        log(&format!("NGLIDE WARNING: no locally installed Glide 2.11 wrapper found. Install nGlide yourself from {OFFICIAL_URL} or place your own glide.dll beside ScorchedPlanetFix.exe; --verify/--launch will refuse until game/splanet/glide.dll exists."));
        return Ok(false);
    };
    let mut src=fs::File::open(&source).map_err(|e|e.to_string())?;
    let mut dest=OpenOptions::new().write(true).create_new(true).open(&output)
        .map_err(|e|format!("Copie du Glide du propriétaire impossible : {e}"))?;
    std::io::copy(&mut src,&mut dest).map_err(|e|e.to_string())?;
    dest.flush().map_err(|e|e.to_string())?;
    dest.sync_all().map_err(|e|e.to_string())?;
    validate_glide_dll(&output)?;
    log(&format!("NGLIDE: user-installed 32-bit glide.dll copied into game directory from {}",source.display()));
    Ok(true)
}
pub fn verify_local_glide(game:&Path)->Result<(),String>{
    validate_glide_dll(&game.join("glide.dll")).map_err(|e|
        format!("{e}. nGlide est requis dans le dossier splanet. Télécharger et installer séparément : {OFFICIAL_URL}. Le logiciel libre ScorchedPlanetFix ne distribue aucune DLL nGlide."))
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
pub fn configure_process(command:&mut Command,mut log:impl FnMut(&str)){
    let backend=if has_vulkan_loader(){"2"}else{"0"};
    for (name,value) in [
        ("NGLIDE_BACKEND",backend), // Vulkan when loader present, otherwise automatic
        ("NGLIDE_RESOLUTION","1"), // By desktop
        ("NGLIDE_ASPECT","1"), // Aspect correction
        ("NGLIDE_REFRESH","2"), // 60 Hz
        ("NGLIDE_VSYNC","1"), // On: minimize tearing / runaway frame rate
        ("NGLIDE_GAMMA","5"), // 1.0
        ("NGLIDE_SPLASH","0"), // Avoid 3dfx splash; nGlide 2.10 had Scorched Planet splash bug
    ]{command.env(name,value);}
    log(&format!("NGLIDE: process-local settings (registry unchanged): backend={} (2=Vulkan loader found; 0=auto), resolution=desktop, aspect=correction, 60Hz, vsync=on, gamma=1.0, splash=off",backend));
}
#[cfg(test)]
mod tests{
    use super::*;
    #[test] fn no_network_or_bundled_wrapper_required(){
        assert!(OFFICIAL_URL.starts_with("https://"));
        let root=std::env::temp_dir().join(format!("nglide-test-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        assert!(verify_local_glide(&root).is_err());
        fs::remove_dir_all(root).unwrap();
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
