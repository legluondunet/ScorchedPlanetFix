// SPDX-License-Identifier: GPL-3.0-or-later
//! Launcher-owned ScorchedPlanetFix.ini. The game's native splanet.ini is
//! independent. [Graphics] applies to both renderers; [Matrox] and [Cheats]
//! retain their own feature namespaces. Legacy [scorch] FPSLimit is migrated.
use std::fs::{self,OpenOptions};
use std::io::Write;
use std::path::Path;

const MAX_INI_SIZE:usize=256*1024;
pub const DEFAULT_FPS:u32=60;
pub const MAX_FPS:u32=240;

#[derive(Debug,Clone,Copy,PartialEq,Eq)]
pub struct GameSettings{
    pub fps_limit:u32,
    pub dither:bool,
    pub unlimited_fuel:bool,
}
impl Default for GameSettings {
    fn default()->Self{Self{fps_limit:DEFAULT_FPS,dither:false,unlimited_fuel:false}}
}
#[derive(Default)]
struct Presence{
    graphics:bool,matrox:bool,cheats:bool,
    graphics_fps:bool,legacy_fps:bool,dither:bool,unlimited_fuel:bool,
    legacy_value:Option<u32>,
}
fn read_bytes(path:&Path)->Result<Option<Vec<u8>>,String>{
    match fs::symlink_metadata(path){
        Ok(meta)=>{
            if !meta.is_file()||meta.file_type().is_symlink(){
                return Err(format!("Configuration is not a regular file: {}",path.display()));
            }
            if meta.len()>MAX_INI_SIZE as u64{return Err("ScorchedPlanetFix.ini exceeds 256 KiB".into());}
            let data=fs::read(path).map_err(|e|e.to_string())?;
            if data.len()>MAX_INI_SIZE{return Err("ScorchedPlanetFix.ini exceeds 256 KiB".into());}
            Ok(Some(data))
        },
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>Ok(None),
        Err(e)=>Err(format!("Cannot inspect INI {}: {e}",path.display())),
    }
}
fn key<'a>(line:&'a [u8])->Option<(&'a [u8],&'a [u8])>{
    let pos=line.iter().position(|c|*c==b'=')?;
    Some((line[..pos].trim_ascii(),line[pos+1..].trim_ascii()))
}
fn fps(value:&[u8])->Result<u32,String>{
    if value.is_empty()||!value.iter().all(u8::is_ascii_digit){
        return Err("INI: FPSLimit must be decimal 0..240".into());
    }
    let x=std::str::from_utf8(value).map_err(|e|e.to_string())?
        .parse::<u32>().map_err(|e|e.to_string())?;
    if x>MAX_FPS{return Err(format!("INI: FPSLimit must be 0..{MAX_FPS}"));}
    Ok(x)
}
fn switch(value:&[u8],name:&str)->Result<bool,String>{
    match value{
        b"0"=>Ok(false),b"1"=>Ok(true),
        _=>Err(format!("INI: {name} must be 0 or 1")),
    }
}
fn section(line:&[u8])->u8{
    if line.eq_ignore_ascii_case(b"[Graphics]"){1}
    else if line.eq_ignore_ascii_case(b"[scorch]"){2}
    else if line.eq_ignore_ascii_case(b"[matrox]"){3}
    else if line.eq_ignore_ascii_case(b"[cheats]"){4}
    else {0}
}
fn parse(data:&[u8])->Result<(GameSettings,Presence),String>{
    let mut result=GameSettings::default();
    let mut seen=Presence::default();
    let mut current=0u8;
    let mut graphics_fps=None;
    for raw in data.split(|c|*c==b'\n'){
        let line=raw.trim_ascii();
        if line.is_empty()||line.starts_with(b";")||line.starts_with(b"#"){continue;}
        if line.starts_with(b"[")&&line.ends_with(b"]"){
            current=section(line);
            match current{1=>seen.graphics=true,3=>seen.matrox=true,4=>seen.cheats=true,_=>{}}
            continue;
        }
        let Some((name,value))=key(line) else {continue};
        match current{
            1 if name.eq_ignore_ascii_case(b"FPSLimit")=>{
                if seen.graphics_fps{return Err("INI: duplicate [Graphics] FPSLimit".into());}
                seen.graphics_fps=true;
                graphics_fps=Some(fps(value)?);
            },
            2 if name.eq_ignore_ascii_case(b"FPSLimit")=>{
                if seen.legacy_fps{return Err("INI: duplicate legacy [scorch] FPSLimit".into());}
                seen.legacy_fps=true;
                seen.legacy_value=Some(fps(value)?);
            },
            3 if name.eq_ignore_ascii_case(b"Dither")=>{
                if seen.dither{return Err("INI: duplicate [matrox] Dither".into());}
                seen.dither=true;result.dither=switch(value,"[matrox] Dither")?;
            },
            4 if name.eq_ignore_ascii_case(b"UnlimitedFuel")=>{
                if seen.unlimited_fuel{return Err("INI: duplicate [Cheats] UnlimitedFuel".into());}
                seen.unlimited_fuel=true;
                result.unlimited_fuel=switch(value,"[Cheats] UnlimitedFuel")?;
            },
            _=>{},
        }
    }
    result.fps_limit=graphics_fps.or(seen.legacy_value).unwrap_or(DEFAULT_FPS);
    Ok((result,seen))
}
pub fn read_settings(path:&Path)->Result<GameSettings,String>{
    match read_bytes(path)?{
        Some(bytes)=>Ok(parse(&bytes)?.0),
        None=>Ok(GameSettings::default()),
    }
}
fn append(out:&mut Vec<u8>,line:&[u8]){
    if !out.is_empty()&&!out.ends_with(b"\n"){out.extend_from_slice(b"\r\n");}
    out.extend_from_slice(line);out.extend_from_slice(b"\r\n");
}
/// Create missing defaults and migrate the former [scorch] FPSLimit to
/// [Graphics], preserving the actual player choice, comments, unrelated
/// sections, and arbitrary legacy non-UTF8 bytes. Called only in verified
/// normal launch / install. A read-only --verify never calls this function.
pub fn ensure_defaults(path:&Path,matrox:bool)->Result<(),String>{
    let existing=read_bytes(path)?;
    let original=existing.as_deref().unwrap_or(b"");
    let (before,seen)=parse(original)?;
    let add_fps=!seen.graphics_fps;
    let add_dither=matrox&&!seen.dither;
    let add_fuel=!seen.unlimited_fuel;
    if !add_fps&&!add_dither&&!add_fuel&&!seen.legacy_fps{return Ok(());}
    let fpsline=format!("FPSLimit={}",before.fps_limit);
    let mut out=Vec::with_capacity(original.len()+160);
    let mut current=0u8;
    let mut did_fps=false;
    let mut did_dither=false;
    let mut did_fuel=false;
    let mut finish=|old:u8,out:&mut Vec<u8>|{
        if old==1&&add_fps&&!did_fps{append(out,fpsline.as_bytes());did_fps=true;}
        if old==3&&add_dither&&!did_dither{append(out,b"Dither=0");did_dither=true;}
        if old==4&&add_fuel&&!did_fuel{append(out,b"UnlimitedFuel=0");did_fuel=true;}
    };
    for line in original.split_inclusive(|b|*b==b'\n'){
        let trimmed=line.trim_ascii();
        if trimmed.starts_with(b"[")&&trimmed.ends_with(b"]"){
            finish(current,&mut out);
            current=section(trimmed);
        }
        // Remove only the obsolete FPSLimit assignment from [scorch].
        // All other native or historical [scorch] settings remain intact.
        if current==2 && key(trimmed).is_some_and(|(name,_)|name.eq_ignore_ascii_case(b"FPSLimit")){
            continue;
        }
        out.extend_from_slice(line);
    }
    finish(current,&mut out);
    if !seen.graphics{append(&mut out,b"[Graphics]");}
    if add_fps&&!did_fps{append(&mut out,fpsline.as_bytes());}
    if matrox&&!seen.matrox{append(&mut out,b"[matrox]");}
    if add_dither&&!did_dither{append(&mut out,b"Dither=0");}
    if !seen.cheats{append(&mut out,b"[Cheats]");}
    if add_fuel&&!did_fuel{append(&mut out,b"UnlimitedFuel=0");}
    if out.len()>MAX_INI_SIZE{return Err("INI migration exceeds 256 KiB".into());}
    if parse(&out)?.0!=before{return Err("INI migration unexpectedly changed a user setting".into());}
    let temporary=path.with_file_name(format!(".scorchedplanetfix-ini-{}-{}.tmp",
        std::process::id(),
        std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH)
            .map_err(|e|e.to_string())?.as_nanos()));
    let result=(||->Result<(),String>{
        let mut output=OpenOptions::new().write(true).create_new(true).open(&temporary)
            .map_err(|e|format!("Cannot stage new INI: {e}"))?;
        output.write_all(&out).map_err(|e|e.to_string())?;
        output.sync_all().map_err(|e|e.to_string())?;
        if existing.is_some(){
            output.set_permissions(fs::metadata(path).map_err(|e|e.to_string())?.permissions())
                .map_err(|e|e.to_string())?;
        }
        drop(output);
        fs::rename(&temporary,path).map_err(|e|format!("Cannot update root INI: {e}"))
    })();
    if result.is_err(){let _=fs::remove_file(&temporary);}
    result
}
/// Do not fake a working cheat: the existing published game cheats do not
/// establish an unlimited-fuel memory or executable patch. Fail explicitly
/// instead of silently launching with an ineffective option.
pub fn reject_unimplemented_cheats(path:&Path)->Result<(),String>{
    if read_settings(path)?.unlimited_fuel {
        return Err("[Cheats] UnlimitedFuel=1 is not yet supported: no verified fuel consumption patch is available for this game executable. Keep UnlimitedFuel=0 until the cheat is implemented.".into());
    }
    Ok(())
}
#[cfg(test)]
mod tests{
    use super::*;
    fn fixture()->std::path::PathBuf{
        let p=std::env::temp_dir().join(format!("scorched-settings-{}-{}",
            std::process::id(),
            std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_nanos()));
        fs::create_dir_all(&p).unwrap();p
    }
    #[test]
    fn graphics_fps_and_matrox_dither_and_cheat_are_separate(){
        let data=b"[Graphics]\nFPSLimit=120\n[matrox]\nDither=1\n[Cheats]\nUnlimitedFuel=1\n";
        assert_eq!(parse(data).unwrap().0,GameSettings{
            fps_limit:120,dither:true,unlimited_fuel:true});
        assert_eq!(parse(b"[Graphics]\nFPSLimit=0\n").unwrap().0.fps_limit,0);
        assert!(parse(b"[Graphics]\nFPSLimit=-1").is_err());
        assert!(parse(b"[Graphics]\nFPSLimit=241").is_err());
        assert!(parse(b"[Graphics]\nFPSLimit=60\nfpslimit=30").is_err());
        assert!(parse(b"[Cheats]\nUnlimitedFuel=2").is_err());
        assert!(parse(b"[Cheats]\nUnlimitedFuel=1\nUnlimitedFuel=0").is_err());
    }
    #[test]
    fn legacy_fps_migrates_without_losing_player_values(){
        let p=fixture();let ini=p.join("ScorchedPlanetFix.ini");
        let original=b"[scorch]\r\nlanguage=2\r\nFPSLimit=75\r\n[Other]\r\nnotes=\xff\r\n[Matrox]\r\nDither=1\r\n";
        fs::write(&ini,original).unwrap();
        ensure_defaults(&ini,true).unwrap();
        let out=fs::read(&ini).unwrap();
        assert!(out.windows(b"notes=\xff".len()).any(|v|v==b"notes=\xff"));
        assert!(out.windows(b"[Graphics]".len()).any(|v|v==b"[Graphics]"));
        assert!(out.windows(b"FPSLimit=75".len()).any(|v|v==b"FPSLimit=75"));
        assert!(out.windows(b"UnlimitedFuel=0".len()).any(|v|v==b"UnlimitedFuel=0"));
        assert_eq!(out.windows(b"FPSLimit=".len()).filter(|s|*s==b"FPSLimit=").count(),1);
        assert_eq!(read_settings(&ini).unwrap(),GameSettings{fps_limit:75,dither:true,unlimited_fuel:false});
        ensure_defaults(&ini,true).unwrap();
        assert_eq!(fs::read(&ini).unwrap(),out);
        fs::remove_dir_all(p).unwrap();
    }
    #[test]
    fn new_ini_has_graphics_matrox_and_disabled_cheat(){
        let p=fixture();let ini=p.join("ScorchedPlanetFix.ini");
        ensure_defaults(&ini,true).unwrap();
        let s=fs::read_to_string(&ini).unwrap();
        assert!(s.contains("[Graphics]\r\nFPSLimit=60"));
        assert!(s.contains("[matrox]\r\nDither=0"));
        assert!(s.contains("[Cheats]\r\nUnlimitedFuel=0"));
        assert!(!s.contains("[scorch]"));
        fs::write(&ini,b"[Graphics]\nFPSLimit=30\n[Cheats]\nUnlimitedFuel=1\n").unwrap();
        assert!(reject_unimplemented_cheats(&ini).unwrap_err().contains("not yet supported"));
        fs::remove_dir_all(p).unwrap();
    }
    #[test]
    fn modern_graphics_overrides_and_removes_legacy_value(){
        let p=fixture();let ini=p.join("ScorchedPlanetFix.ini");
        fs::write(&ini,b"[Graphics]\nFPSLimit=30\n[scorch]\nFPSLimit=60\n[Cheats]\nUnlimitedFuel=0\n").unwrap();
        ensure_defaults(&ini,false).unwrap();
        assert_eq!(read_settings(&ini).unwrap().fps_limit,30);
        assert_eq!(fs::read_to_string(&ini).unwrap().matches("FPSLimit=").count(),1);
        fs::remove_dir_all(p).unwrap();
    }
}
