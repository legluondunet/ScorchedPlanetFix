// SPDX-License-Identifier: GPL-3.0-or-later
//! Launcher-owned ScorchedPlanetFix.ini. The game's native splanet.ini is
//! independent. [Graphics] applies to both renderers; [Matrox] and [Cheats]
//! retain their own feature namespaces. Legacy [scorch] FPSLimit is migrated.
use std::fs::{self,OpenOptions};
use std::io::Write;
use std::path::Path;

const MAX_INI_SIZE:usize=256*1024;
const OPTION_HELP_MARKER: &[u8] = b"; ScorchedPlanetFix option descriptions";
const OPTION_HELP: &[u8] = b"; ScorchedPlanetFix option descriptions\r\n\
; [Graphics] FPSLimit: 0=unlimited, 1..240=maximum FPS for both renderers\r\n\
; [Matrox] Dither: 0=off, 1=ordered dithering when requested by the game (Matrox only)\r\n\
; [Matrox] Icon01Opaque: 0=unchanged, 1=experimental original icon01 black background (signature-locked)\r\n\
; [Matrox] WorldBilinear: 0=1996 original nearest, 1=optional smooth opaque 3D world textures only\r\n\
; [Cheats] UnlimitedFuel: reserved, not supported yet; default=0\r\n\
; [Cheats] ALIAH: invulnerability code, manual in-game only; default=0\r\n\
; [Cheats] FATAL: all weapons and ammunition, manual in-game only; default=0\r\n\
; [Debug] Log: 0=stop runtime logs after first successful play, 1=log all sessions\r\n\
; The first verified game launch always logs once, even with Log=0\r\n\
; Installer log stays enabled regardless of [Debug] Log\r\n\r\n";
pub const DEFAULT_FPS:u32=60;
pub const MAX_FPS:u32=240;

#[derive(Debug,Clone,Copy,PartialEq,Eq)]
pub struct GameSettings{
    pub fps_limit:u32,
    pub dither:bool,
    pub rgb888_lighting:bool,
    pub icon01_opaque:bool,
    pub world_bilinear:bool,
    pub unlimited_fuel:bool,
    pub aliah:bool,
    pub fatal:bool,
    pub log:bool,
}
impl Default for GameSettings {
    fn default()->Self{Self{fps_limit:DEFAULT_FPS,dither:false,icon01_opaque:false,world_bilinear:false,rgb888_lighting:false,unlimited_fuel:false,aliah:false,fatal:false,log:false}}
}
#[derive(Default)]
struct Presence{
    graphics:bool,matrox:bool,cheats:bool,
    graphics_fps:bool,legacy_fps:bool,dither:bool,icon01_opaque:bool,world_bilinear:bool,rgb888_lighting:bool,unlimited_fuel:bool,
    aliah:bool,fatal:bool,debug:bool,log:bool,
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
    else if line.eq_ignore_ascii_case(b"[debug]"){5}
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
            match current{1=>seen.graphics=true,3=>seen.matrox=true,4=>seen.cheats=true,5=>seen.debug=true,_=>{}}
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
            3 if name.eq_ignore_ascii_case(b"RGB888Lighting")=>{
                if seen.rgb888_lighting{return Err("INI: duplicate [Matrox] RGB888Lighting".into());}
                seen.rgb888_lighting=true;result.rgb888_lighting=switch(value,"[Matrox] RGB888Lighting")?;
            },
            3 if name.eq_ignore_ascii_case(b"Icon01Opaque")=>{
                if seen.icon01_opaque{return Err("INI: duplicate [Matrox] Icon01Opaque".into());}
                seen.icon01_opaque=true;result.icon01_opaque=switch(value,"[Matrox] Icon01Opaque")?;
            },
            3 if name.eq_ignore_ascii_case(b"WorldBilinear")=>{
                if seen.world_bilinear{return Err("INI: duplicate [Matrox] WorldBilinear".into());}
                seen.world_bilinear=true;result.world_bilinear=switch(value,"[Matrox] WorldBilinear")?;
            },
            4 if name.eq_ignore_ascii_case(b"UnlimitedFuel")=>{
                if seen.unlimited_fuel{return Err("INI: duplicate [Cheats] UnlimitedFuel".into());}
                seen.unlimited_fuel=true;
                result.unlimited_fuel=switch(value,"[Cheats] UnlimitedFuel")?;
            },
            4 if name.eq_ignore_ascii_case(b"ALIAH")=>{
                if seen.aliah{return Err("INI: duplicate [Cheats] ALIAH".into());}
                seen.aliah=true;result.aliah=switch(value,"[Cheats] ALIAH")?;
            },
            4 if name.eq_ignore_ascii_case(b"FATAL")=>{
                if seen.fatal{return Err("INI: duplicate [Cheats] FATAL".into());}
                seen.fatal=true;result.fatal=switch(value,"[Cheats] FATAL")?;
            },
            5 if name.eq_ignore_ascii_case(b"Log")=>{
                if seen.log{return Err("INI: duplicate [Debug] Log".into());}
                seen.log=true;result.log=switch(value,"[Debug] Log")?;
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
    let original=existing.as_deref().unwrap_or(crate::layout::INI_EXAMPLE.as_bytes());
    let (before,seen)=parse(original)?;
    let add_fps=!seen.graphics_fps;
    let add_dither=matrox&&!seen.dither;
    let add_rgb888=matrox&&!seen.rgb888_lighting;
    let add_icon01_opaque=matrox&&!seen.icon01_opaque;
    let add_world_bilinear=matrox&&!seen.world_bilinear;
    let add_fuel=!seen.unlimited_fuel;
    let add_aliah=!seen.aliah;
    let add_fatal=!seen.fatal;
    let add_log=!seen.log;
    let add_descriptions=!original.windows(OPTION_HELP_MARKER.len())
        .any(|slice|slice==OPTION_HELP_MARKER);
    if existing.is_some()&&!add_descriptions&&!add_fps&&!add_dither&&!add_rgb888&&!add_icon01_opaque&&!add_world_bilinear&&!add_fuel&&!add_aliah&&!add_fatal&&!add_log&&!seen.legacy_fps{return Ok(());}
    let fpsline=format!("FPSLimit={}",before.fps_limit);
    let mut out=Vec::with_capacity(original.len()+OPTION_HELP.len()+160);
    if add_descriptions {out.extend_from_slice(OPTION_HELP);}
    let mut current=0u8;
    let mut did_fps=false;
    let mut did_dither=false;
    let mut did_rgb888=false;
    let mut did_icon01_opaque=false;
    let mut did_world_bilinear=false;
    let mut did_fuel=false;
    let mut did_aliah=false;
    let mut did_fatal=false;
    let mut did_log=false;
    let mut finish=|old:u8,out:&mut Vec<u8>|{
        if old==1&&add_fps&&!did_fps{append(out,fpsline.as_bytes());did_fps=true;}
        if old==3&&add_dither&&!did_dither{append(out,b"Dither=0");did_dither=true;}
        if old==3&&add_rgb888&&!did_rgb888{append(out,b"; Experimental RGB888 /256 lighting; 0=legacy, 1=experimental");append(out,b"RGB888Lighting=0");did_rgb888=true;}
        if old==3&&add_icon01_opaque&&!did_icon01_opaque{
            append(out,b"; Original Matrox title atlas icon01 only; experimental, default off");
            append(out,b"Icon01Opaque=0");did_icon01_opaque=true;
        }
        if old==3&&add_world_bilinear&&!did_world_bilinear{
            append(out,b"; Smooth opaque 3D textures only; original Mystique uses nearest (0)");
            append(out,b"WorldBilinear=0");did_world_bilinear=true;
        }
        if old==4&&add_fuel&&!did_fuel{append(out,b"UnlimitedFuel=0");did_fuel=true;}
        if old==4&&add_aliah&&!did_aliah{append(out,b"ALIAH=0");did_aliah=true;}
        if old==4&&add_fatal&&!did_fatal{append(out,b"FATAL=0");did_fatal=true;}
        if old==5&&add_log&&!did_log{
            append(out,b"; 0 = no renderer log (default); 1 = per-session renderer log next to EXE");
            append(out,b"Log=0");did_log=true;
        }
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
    if add_rgb888&&!did_rgb888{append(&mut out,b"; Experimental RGB888 /256 lighting; 0=legacy, 1=experimental");append(&mut out,b"RGB888Lighting=0");}
    if add_icon01_opaque&&!did_icon01_opaque{
        append(&mut out,b"; Original Matrox title atlas icon01 only; experimental, default off");
        append(&mut out,b"Icon01Opaque=0");
    }
    if add_world_bilinear&&!did_world_bilinear{
        append(&mut out,b"; Smooth opaque 3D textures only; original Mystique uses nearest (0)");
        append(&mut out,b"WorldBilinear=0");
    }
    if !seen.cheats{append(&mut out,b"[Cheats]");}
    if add_fuel&&!did_fuel{append(&mut out,b"UnlimitedFuel=0");}
    if add_aliah&&!did_aliah{append(&mut out,b"ALIAH=0");}
    if add_fatal&&!did_fatal{append(&mut out,b"FATAL=0");}
    if !seen.debug{append(&mut out,b"[Debug]");}
    if add_log&&!did_log{
        append(&mut out,b"; 0 = no renderer log (default); 1 = per-session renderer log next to EXE");
        append(&mut out,b"Log=0");
    }
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
    let configured=read_settings(path)?;
    if configured.unlimited_fuel {
        return Err("[Cheats] UnlimitedFuel=1 is not yet supported: no verified fuel consumption patch is available for this game executable. Keep UnlimitedFuel=0 until the cheat is implemented.".into());
    }
    // ALIAH and FATAL are established in-game keyboard codes, but there is
    // no authenticated automated key-injection path for either retail or
    // Matrox yet. Never silently pretend the INI toggle activated the cheat.
    if configured.aliah || configured.fatal {
        let codes=match (configured.aliah,configured.fatal){
            (true,true)=>"ALIAH and FATAL",
            (true,false)=>"ALIAH",
            (false,true)=>"FATAL",
            (false,false)=>unreachable!(),
        };
        return Err(format!(
            "[Cheats] {codes}=1 requests automatic cheat activation, which is not implemented yet. Enter the codes manually during gameplay (invulnerability may require ALLAH instead of ALIAH depending on the game build), or set these INI options back to 0."));
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
        let data=b"[Graphics]\nFPSLimit=120\n[matrox]\nDither=1\n[Cheats]\nUnlimitedFuel=1\nALIAH=1\nFATAL=1\n";
        assert_eq!(parse(data).unwrap().0,GameSettings{
            fps_limit:120,dither:true,icon01_opaque:false,world_bilinear:false,rgb888_lighting:false,unlimited_fuel:true,aliah:true,fatal:true,log:false});
        assert_eq!(parse(b"[Graphics]\nFPSLimit=0\n").unwrap().0.fps_limit,0);
        assert!(parse(b"[Graphics]\nFPSLimit=-1").is_err());
        assert!(parse(b"[Graphics]\nFPSLimit=241").is_err());
        assert!(parse(b"[Graphics]\nFPSLimit=60\nfpslimit=30").is_err());
        assert!(parse(b"[Matrox]\nRGB888Lighting=2").is_err());
        assert!(parse(b"[Matrox]\nRGB888Lighting=1\nrgb888lighting=0").is_err());
        let experimental=parse(b"[Matrox]\nRGB888Lighting=1\nDither=0").unwrap().0;
        assert!(experimental.rgb888_lighting && !experimental.dither);
        assert!(!parse(b"").unwrap().0.rgb888_lighting);
        assert!(parse(b"[Matrox]\nWorldBilinear=2").is_err());
        assert!(parse(b"[Matrox]\nWorldBilinear=1\nworldbilinear=0").is_err());
        assert!(parse(b"[Matrox]\nWorldBilinear=1").unwrap().0.world_bilinear);
        assert!(!parse(b"[Matrox]\nWorldBilinear=0").unwrap().0.world_bilinear);
        assert!(parse(b"[Matrox]\nIcon01Opaque=2").is_err());
        assert!(parse(b"[Matrox]\nIcon01Opaque=1\nicon01opaque=0").is_err());
        assert!(parse(b"[Matrox]\nIcon01Opaque=1").unwrap().0.icon01_opaque);
        assert!(!parse(b"[Matrox]\nIcon01Opaque=0").unwrap().0.icon01_opaque);
        assert!(parse(b"[Cheats]\nUnlimitedFuel=2").is_err());
        assert!(parse(b"[Cheats]\nUnlimitedFuel=1\nUnlimitedFuel=0").is_err());
        assert!(parse(b"[Cheats]\nALIAH=2").is_err());
        assert!(parse(b"[Cheats]\nFATAL=YES").is_err());
        assert!(parse(b"[Cheats]\naliah=0\nALIAH=1").is_err());
        assert!(parse(b"[Cheats]\nFATAL=0\nfatal=1").is_err());
        assert_eq!(parse(b"[Debug]\nLog=1").unwrap().0.log,true);
        assert!(!parse(b"").unwrap().0.log);
        assert!(parse(b"[Debug]\nLog=2").is_err());
        assert!(parse(b"[Debug]\nLog=0\nlog=1").is_err());
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
        assert!(out.windows(b"ALIAH=0".len()).any(|v|v==b"ALIAH=0"));
        assert!(out.windows(b"FATAL=0".len()).any(|v|v==b"FATAL=0"));
        assert!(out.windows(b"Icon01Opaque=0".len()).any(|v|v==b"Icon01Opaque=0"));
        assert!(out.windows(b"WorldBilinear=0".len()).any(|v|v==b"WorldBilinear=0"));
        assert!(out.windows(b"RGB888Lighting=0".len()).any(|v|v==b"RGB888Lighting=0"));
        assert!(out.windows(b"[Debug]".len()).any(|v|v==b"[Debug]"));
        assert!(out.windows(b"Log=0".len()).any(|v|v==b"Log=0"));
        assert_eq!(out.windows(b"FPSLimit=".len()).filter(|s|*s==b"FPSLimit=").count(),1);
        assert_eq!(read_settings(&ini).unwrap(),GameSettings{fps_limit:75,dither:true,icon01_opaque:false,world_bilinear:false,rgb888_lighting:false,unlimited_fuel:false,aliah:false,fatal:false,log:false});
        ensure_defaults(&ini,true).unwrap();
        assert_eq!(fs::read(&ini).unwrap(),out);
        fs::remove_dir_all(p).unwrap();
    }
    #[test]
    fn new_ini_has_graphics_matrox_and_disabled_cheat(){
        let p=fixture();let ini=p.join("ScorchedPlanetFix.ini");
        ensure_defaults(&ini,true).unwrap();
        let s=fs::read_to_string(&ini).unwrap();
        assert!(s.contains("[Graphics]\r\n"));
        assert!(s.contains("FPSLimit=60"));
        assert!(s.contains("[Matrox]\r\n"));
        assert!(s.contains("Dither=0"));
        assert!(s.contains("RGB888Lighting=0"));
        assert!(s.contains("Icon01Opaque=0"));
        assert!(s.contains("WorldBilinear=0"));
        assert!(s.contains("[Cheats]\r\n"));
        assert!(s.contains("UnlimitedFuel=0"));
        assert!(s.contains("ALIAH=0"));
        assert!(s.contains("FATAL=0"));
        assert!(s.contains("[Debug]\r\n"));
        assert!(s.contains("Log=0"));
        assert!(s.contains("; ScorchedPlanetFix option descriptions"));
        assert!(!s.contains("[scorch]"));
        fs::write(&ini,b"[Graphics]\nFPSLimit=30\n[Cheats]\nUnlimitedFuel=1\n").unwrap();
        assert!(reject_unimplemented_cheats(&ini).unwrap_err().contains("not yet supported"));
        fs::remove_dir_all(p).unwrap();
    }
    #[test]
    fn existing_cheats_and_unrelated_ini_keys_are_preserved() {
        let p=fixture();
        let ini=p.join("ScorchedPlanetFix.ini");
        fs::write(&ini,b"[Graphics]\nFPSLimit=45\n[Cheats]\nALIAH=1\nUserKey=preserve\nFATAL=0\n").unwrap();
        ensure_defaults(&ini,false).unwrap();
        let contents=fs::read_to_string(&ini).unwrap();
        assert!(contents.contains("ALIAH=1"));
        assert!(contents.contains("FATAL=0"));
        assert!(contents.contains("UserKey=preserve"));
        assert!(contents.contains("UnlimitedFuel=0"));
        assert_eq!(contents.matches("ALIAH=").count(),1);
        assert_eq!(contents.matches("FATAL=").count(),1);
        let error=reject_unimplemented_cheats(&ini).unwrap_err();
        assert!(error.contains("ALIAH"));
        fs::write(&ini,b"[Cheats]\nFATAL=1\n").unwrap();
        assert!(reject_unimplemented_cheats(&ini).unwrap_err().contains("FATAL"));
        fs::remove_dir_all(p).unwrap();
    }
    #[test]
    fn user_debug_log_is_preserved_and_missing_debug_defaults_off(){
        let dir=fixture();let ini=dir.join("ScorchedPlanetFix.ini");
        fs::write(&ini,b"[Debug]\r\nLog=1\r\n[Graphics]\r\nFPSLimit=30\r\n").unwrap();
        ensure_defaults(&ini,true).unwrap();
        assert!(read_settings(&ini).unwrap().log);
        assert_eq!(fs::read_to_string(&ini).unwrap().lines()
            .filter(|line|line.trim_start().starts_with("Log=")).count(),1);
        let before=fs::read(&ini).unwrap();
        ensure_defaults(&ini,true).unwrap();
        assert_eq!(fs::read(&ini).unwrap(),before);
        fs::write(&ini,b"[Graphics]\r\nFPSLimit=60\r\n").unwrap();
        ensure_defaults(&ini,false).unwrap();
        assert!(!read_settings(&ini).unwrap().log);
        let description=fs::read_to_string(&ini).unwrap();
        assert!(description.contains("[Debug]\r\n"));
        assert!(description.contains("; ScorchedPlanetFix option descriptions"));
        ensure_defaults(&ini,false).unwrap();
        assert_eq!(fs::read_to_string(&ini).unwrap(),description);
        fs::remove_dir_all(dir).unwrap();
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
