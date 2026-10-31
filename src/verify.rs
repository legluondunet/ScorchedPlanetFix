// SPDX-License-Identifier: GPL-3.0-or-later
//! Read-only, strict install verification before any game launch.
//! Uses owner-supplied SHA-256 reference inventories for immutable game assets.
//! Save slots (*.sav) and splanet.ini are mutable, so validate structure and
//! safety rather than requiring their original hashes.
use crate::verify_manifest::{manifest_for, ReferenceFile};
use crate::nglide;
use sha2::{Digest,Sha256};
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path,PathBuf};
use std::process::{Child,Command};
use scorched_planet_fix::{self,profile_from_hash};

pub struct Report {
    pub edition:&'static str,
    pub checked_files:usize,
    pub checked_bytes:u64,
    pub executable:PathBuf,
}
fn regular_directory(path:&Path)->Result<(),String>{
    let meta=fs::symlink_metadata(path).map_err(|e|format!("Directory missing {}: {e}",path.display()))?;
    if !meta.is_dir() || meta.file_type().is_symlink() {
        return Err(format!("Invalid directory or symlink: {}",path.display()));
    }
    Ok(())
}
fn regular_file(path:&Path)->Result<(),String>{
    let meta=fs::symlink_metadata(path).map_err(|e|format!("File missing {}: {e}",path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink() {
        return Err(format!("Invalid file or symlink: {}",path.display()));
    }
    Ok(())
}
fn resolve_case_fold(root:&Path,relative:&str)->Result<PathBuf,String>{
    let mut current=root.to_path_buf();
    for name in relative.split('/') {
        if name.is_empty() || name=="." || name==".." || !name.is_ascii() || name.contains(['\\',':']) {
            return Err(format!("Invalid manifest path: {relative}"));
        }
        regular_directory(&current)?;
        let mut found=Vec::new();
        for entry in fs::read_dir(&current).map_err(|e|format!("Failed to read {}: {e}",current.display()))?{
            let entry=entry.map_err(|e|e.to_string())?;
            if entry.file_name().to_string_lossy().eq_ignore_ascii_case(name){
                found.push(entry.path());
            }
        }
        if found.len()!=1 {return Err(format!("File missing or ambiguous: {relative}"));}
        current=found.remove(0);
        let meta=fs::symlink_metadata(&current).map_err(|e|e.to_string())?;
        if meta.file_type().is_symlink(){return Err(format!("Symlink not allowed: {}",current.display()));}
    }
    regular_file(&current)?;
    Ok(current)
}
fn hash_file(path:&Path)->Result<(u64,String),String>{
    regular_file(path)?;
    let mut file=File::open(path).map_err(|e|format!("Failed to read {}: {e}",path.display()))?;
    let mut hash=Sha256::new();
    let mut size=0u64;
    let mut buffer=[0u8;64*1024];
    loop {
        let n=file.read(&mut buffer).map_err(|e|e.to_string())?;
        if n==0 {break;}
        size=size.checked_add(n as u64).ok_or("File size too large")?;
        hash.update(&buffer[..n]);
    }
    Ok((size,format!("{:x}",hash.finalize())))
}
fn read_scorch_ini(path:&Path)->Result<(String,Option<u8>),String>{
    regular_file(path)?;
    let bytes=fs::read(path).map_err(|e|format!("Could not read INI: {e}"))?;
    if bytes.len()>256*1024{return Err("splanet.ini is too large".into());}
    let mut section=false;
    let mut videos=None;
    let mut language=None;
    for line in bytes.split(|b|*b==b'\n') {
        let text=line.trim_ascii();
        if text.starts_with(b"[") && text.ends_with(b"]"){
            section=text.eq_ignore_ascii_case(b"[scorch]");
            continue;
        }
        if !section || text.starts_with(b";") || text.starts_with(b"#"){continue;}
        let Some(position)=text.iter().position(|c|*c==b'=') else {continue};
        let (key,value)=text.split_at(position);
        let val=std::str::from_utf8(value[1..].trim_ascii())
            .map_err(|_|"INI: non-ASCII value in required setting")?;
        if key.trim_ascii().eq_ignore_ascii_case(b"video"){
            if videos.is_some() {return Err("INI: video is defined more than once".into());}
            videos=Some(val.to_string());
        }else if key.trim_ascii().eq_ignore_ascii_case(b"language"){
            if language.is_some() {return Err("INI: language is defined more than once".into());}
            language=Some(val.parse::<u8>().map_err(|_|"INI: invalid language code")?);
        }
    }
    let video=videos.ok_or("INI: missing [scorch] video=")?;
    Ok((video,language))
}
fn allowed_language(label:&str,lang:u8)->bool{
    match label{
        "US 1.08"=>lang==1,
        "EN/ES/IT 1.06"=>matches!(lang,1|4|5),
        "FR/DE 1.06 (FadeLow + CD check fix)" | "FR/DE 1.06 (alternate CD executable)"=>matches!(lang,2|3),
        _=>false
    }
}
/// Return only exact, already patched profiles, for safe legacy INI migration.
pub fn profile_for_patched_exe(exe:&Path)->Result<scorched_planet_fix::FadeProfile,String>{
    let (_,hash)=hash_file(exe)?;
    let profile=profile_from_hash(&hash)
        .ok_or_else(||format!("Unknown executable (SHA-256 {hash})"))?;
    if hash!=profile.patched_hash {return Err("The game executable has not been patched yet".into());}
    Ok(profile)
}
/// Used only by installation/repair workflows, never by read-only
/// verification. Preserve a preselected valid edition language when
/// migrating older absolute-video INI files to video= video.
pub fn preferred_language(folder:&Path,label:&str)->u8 {
    let default=if label.starts_with("FR/DE") {2} else {1};
    read_scorch_ini(&folder.join("splanet.ini")).ok()
        .and_then(|(_,lang)|lang)
        .filter(|code|allowed_language(label,*code))
        .unwrap_or(default)
}
fn inspect_file(root:&Path, entry:&ReferenceFile)->Result<u64,String>{
    let path=resolve_case_fold(root,entry.path)?;
    let (size,digest)=hash_file(&path)?;
    if size!=entry.size || digest!=entry.sha256{
        return Err(format!("Altered or incompatible game file: {} (size {}, SHA-256 {})",
            entry.path,size,digest));
    }
    Ok(size)
}
/// Verify a playable, *patched* edition. This function never modifies data.
/// Original executable is required in its same-name .bak backup.
pub fn verify_install(folder:&Path,mut log:impl FnMut(&str))->Result<Report,String>{
    regular_directory(folder)?;
    log("VERIFY: starting full immutable-file SHA-256 audit");
    let exe=scorched_planet_fix::find_installed_exe(folder)?;
    let (_,digest)=hash_file(&exe)?;
    let profile=profile_from_hash(&digest)
        .ok_or_else(||format!("Unrecognized executable: SHA-256 {digest}"))?;
    if digest!=profile.patched_hash{
        return Err(format!("Executable {} is not patched. Run ScorchedPlanetFix.exe without arguments to apply the patch.",
            profile.label));
    }
    // A backup must match the exact edition: never blindly trust a .bak.
    // Construct backup from the file name to stay in its own directory.
    let mut name=exe.file_name().ok_or("Missing executable filename")?.to_os_string();
    name.push(".bak");
    let backup=exe.with_file_name(name);
    let (_,backup_hash)=hash_file(&backup)?;
    if backup_hash!=profile.original_hash {
        return Err(format!("Original backup does not match expected file: {} (SHA-256 {backup_hash})",
            backup.display()));
    }
    let (video,language)=read_scorch_ini(&resolve_case_fold(folder,"splanet.ini")?)?;
    if !video.eq_ignore_ascii_case("video"){
        return Err(format!("INI: video must be relative (video= video), currently {video:?}. Fix the INI before launching."));
    }
    if let Some(lang)=language {
        if !allowed_language(profile.label,lang){
            return Err(format!("INI: language={lang} is not compatible with edition {}",profile.label));
        }
    }
    // Neither original software renderer is needed for the 3dfx edition.
    // Reject them at verification time. The installer removes exact original
    // files from older installations but never deletes unknown replacements.
    for item in fs::read_dir(folder).map_err(|e|e.to_string())?{
        let item=item.map_err(|e|e.to_string())?;
        let name=item.file_name().to_string_lossy().to_ascii_lowercase();
        if matches!(name.as_str(),"splanet.exe"|"splanetw.exe"){
            return Err(format!("Unused {name} is present: run ScorchedPlanetFix.exe without arguments to remove it if its checksum matches the original CD."));
        }
    }
    // nGlide is a runtime prerequisite: the check is read-only and never
    // downloads/installs the proprietary DLL.
    nglide::verify_local_glide(folder)?;
    let data=manifest_for(profile.label).ok_or("Edition has no verification manifest")?;
    let mut total=0u64;
    for entry in data{
        total+=inspect_file(folder,entry)?;
    }
    // Preserve user's game saves (original SP00...SP07.SAV can change during
    // gameplay), and do not require the mutable INI to match original media.
    log(&format!("VERIFY OK: edition={} executable={} original_backup={} files={} bytes={} video= video",
        profile.label,digest,backup_hash,data.len(),total));
    Ok(Report{
        edition:profile.label,
        checked_files:data.len()+2, // exe and original backup
        checked_bytes:total,
        executable:exe,
    })
}
pub fn launch_verified(folder:&Path,show_3dfx_logo:bool,mut log:impl FnMut(&str))->Result<Child,String>{
    let report=verify_install(folder,|message|log(message))?;
    log(&format!("LAUNCH: verified {} immutable files for {} before starting game",
        report.checked_files,report.edition));
    if show_3dfx_logo{
        nglide::check_splash_plugin(folder,|message|log(message))?;
    }
    let mut command=Command::new(&report.executable);
    command.current_dir(folder);
    nglide::configure_process(&mut command,show_3dfx_logo,|message|log(message));
    command.spawn()
        .map_err(|e|format!("Unable to launch {}: {e}",report.executable.display()))
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn detect_edition_manifest_and_supported_languages(){
        for (edition,files,min) in [
            ("US 1.08",manifest_for("US 1.08").unwrap(),80usize),
            ("EN/ES/IT 1.06",manifest_for("EN/ES/IT 1.06").unwrap(),140),
            ("FR/DE 1.06 (FadeLow + CD check fix)",
                manifest_for("FR/DE 1.06 (FadeLow + CD check fix)").unwrap(),100),
            ("FR/DE 1.06 (alternate CD executable)",manifest_for("FR/DE 1.06 (alternate CD executable)").unwrap(),100)]{
            assert!(files.len()>min,"{edition}");
            assert!(files.iter().any(|x|x.path=="geometry.rwb"));
            assert!(files.iter().any(|x|x.path=="rwdlvood.dll"));
            assert!(files.iter().any(|x|x.path.starts_with("video/")));
            assert!(!files.iter().any(|x|x.path.ends_with(".sav")));
            for name in ["splanet.exe","splanetw.exe"]{
                assert!(!files.iter().any(|x|x.path==name),
                    "unused software renderer {name} must not be required for {edition}");
            }
        }
        assert!(allowed_language("US 1.08",1));
        assert!(allowed_language("EN/ES/IT 1.06",5));
        assert!(!allowed_language("EN/ES/IT 1.06",2));
        assert!(allowed_language("FR/DE 1.06 (FadeLow + CD check fix)",3));
        assert!(allowed_language("FR/DE 1.06 (alternate CD executable)",2));
        assert!(allowed_language("FR/DE 1.06 (alternate CD executable)",3));
        assert!(!allowed_language("FR/DE 1.06 (alternate CD executable)",1));
        assert!(std::ptr::eq(manifest_for("FR/DE 1.06 (alternate CD executable)").unwrap(),
            manifest_for("FR/DE 1.06 (FadeLow + CD check fix)").unwrap()));
    }
    #[test] fn accepts_minimal_relative_video_ini(){
        let dir=std::env::temp_dir().join(format!("verify-ini-{}",std::process::id()));
        let _=fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        let path=dir.join("splanet.ini");
        fs::write(&path,b"[scorch]\r\nvideo= video\r\n").unwrap();
        assert_eq!(read_scorch_ini(&path).unwrap(),("video".into(),None));
        fs::write(&path,b"[scorch]\nvideo= video\nlanguage=4\n").unwrap();
        assert_eq!(read_scorch_ini(&path).unwrap(),("video".into(),Some(4)));
        fs::write(&path,b"[scorch]\nvideo= video\nvideo=c:\\old\n").unwrap();
        assert!(read_scorch_ini(&path).is_err());
        fs::remove_dir_all(dir).unwrap();
    }
    #[test] fn missing_files_and_case_collisions_are_rejected(){
        let dir=std::env::temp_dir().join(format!("verify-case-{}",std::process::id()));
        let _=fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        assert!(verify_install(&dir,|_|{}).is_err());
        fs::write(dir.join("Geometry.RWB"),b"test").unwrap();
        assert_eq!(resolve_case_fold(&dir,"geometry.rwb").unwrap(),dir.join("Geometry.RWB"));
        #[cfg(not(windows))]
        {
            fs::write(dir.join("geometry.rwb"),b"test").unwrap();
            assert!(resolve_case_fold(&dir,"geometry.rwb").is_err());
        }
        fs::remove_dir_all(dir).unwrap();
    }
}
