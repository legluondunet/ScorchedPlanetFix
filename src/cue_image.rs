// SPDX-License-Identifier: GPL-3.0-or-later
//! Read-only CUE/BIN data-track normalization for the existing ISO filesystem parser.
//! Never write to the input CUE/BIN or include audio tracks in the extracted game files.
//! Only first-track MODE1/2048, MODE1/2352 and MODE2/2352 (XA Form 1) are supported.
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Component, Path, PathBuf};

const SECTOR_DATA: usize = 2048;
const RAW_SECTOR: usize = 2352;
const MAX_CUE_BYTES: u64 = 256 * 1024;
const MAX_IMAGE_BYTES: u64 = 8 * 1024 * 1024 * 1024;
const SYNC: [u8;12] = [0,255,255,255,255,255,255,255,255,255,255,0];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum TrackMode { Mode1_2048, Mode1_2352, Mode2_2352, Audio, Unsupported }
impl TrackMode {
    fn parse(s:&str)->Self {
        match s.to_ascii_uppercase().as_str() {
            "MODE1/2048"=>Self::Mode1_2048,
            "MODE1/2352"=>Self::Mode1_2352,
            "MODE2/2352"=>Self::Mode2_2352,
            "AUDIO"=>Self::Audio,
            _=>Self::Unsupported,
        }
    }
    fn sector_bytes(self)->usize {
        if self==Self::Mode1_2048 {SECTOR_DATA} else {RAW_SECTOR}
    }
    fn user_offset(self)->usize {
        match self {
            Self::Mode1_2048=>0,
            Self::Mode1_2352=>16,
            Self::Mode2_2352=>24,
            _=>0,
        }
    }
}
#[derive(Clone, Debug)]
struct Track {
    number: u8,
    file: String,
    file_type: String,
    mode: TrackMode,
    index00: Option<u64>,
    index01: Option<u64>,
}
#[derive(Debug)]
struct DataTrack {
    file: PathBuf,
    mode: TrackMode,
    first_sector: u64,
    sector_count: u64,
}

fn msf_frames(s:&str)->Result<u64,String> {
    let parts:Vec<_>=s.split(':').collect();
    if parts.len()!=3 || parts.iter().any(|p|p.len()!=2 || !p.bytes().all(|b|b.is_ascii_digit())) {
        return Err(format!("Horodatage CUE invalide : {s}"));
    }
    let min=parts[0].parse::<u64>().map_err(|e|e.to_string())?;
    let sec=parts[1].parse::<u64>().map_err(|e|e.to_string())?;
    let frame=parts[2].parse::<u64>().map_err(|e|e.to_string())?;
    if sec>=60 || frame>=75 {return Err(format!("Temps MSF CUE invalide : {s}"));}
    Ok(min*60*75+sec*75+frame)
}
fn parse_file_arg(raw:&str)->Result<(String,String),String>{
    let raw=raw.trim();
    let (filename, tail)=if let Some(rest)=raw.strip_prefix('"') {
        let i=rest.find('"').ok_or("Guillemet FILE non fermé dans le CUE")?;
        (rest[..i].to_string(),rest[i+1..].trim())
    }else{
        let mut parts=raw.splitn(2,char::is_whitespace);
        (parts.next().unwrap_or("").to_string(),parts.next().unwrap_or("").trim())
    };
    if filename.is_empty() {return Err("FILE CUE sans nom".into());}
    let mut parts=tail.split_whitespace();
    let kind=parts.next().ok_or("FILE CUE sans type")?;
    if parts.next().is_some() {return Err("FILE CUE avec arguments imprévus".into());}
    Ok((filename,kind.to_ascii_uppercase()))
}
fn parse_cue(text:&str)->Result<Vec<Track>,String>{
    let mut tracks:Vec<Track>=Vec::new();
    let mut current_file:Option<(String,String)>=None;
    for (line_number,original) in text.trim_start_matches('\u{feff}').lines().enumerate(){
        let line=original.trim();
        if line.is_empty() {continue;}
        let mut parts=line.split_whitespace();
        let keyword=parts.next().unwrap().to_ascii_uppercase();
        match keyword.as_str(){
            "REM"|"TITLE"|"PERFORMER"|"SONGWRITER"|"CATALOG"|"ISRC"|"FLAGS"|
            "PREGAP"|"POSTGAP"|"CDTEXTFILE" => {},
            "FILE" =>{
                let args=line[keyword.len()..].trim();
                current_file=Some(parse_file_arg(args)?);
            },
            "TRACK" =>{
                let number=parts.next().ok_or("TRACK sans numéro")?
                    .parse::<u8>().map_err(|_|"Numéro TRACK incorrect")?;
                let mode=TrackMode::parse(parts.next().ok_or("TRACK sans mode")?);
                if parts.next().is_some() || number==0 || tracks.len()>=99 ||
                    number!=tracks.len() as u8+1 {
                    return Err(format!("Ordre ou numéro TRACK incorrect, ligne {}",line_number+1));
                }
                let (file,file_type)=current_file.clone().ok_or("TRACK avant FILE dans CUE")?;
                tracks.push(Track{number,file,file_type,mode,index00:None,index01:None});
            },
            "INDEX" =>{
                let track=tracks.last_mut().ok_or("INDEX avant TRACK dans CUE")?;
                let index=parts.next().ok_or("INDEX sans numéro")?
                    .parse::<u8>().map_err(|_|"Numéro INDEX incorrect")?;
                let time=msf_frames(parts.next().ok_or("INDEX sans temps")?)?;
                if parts.next().is_some(){return Err("INDEX avec arguments en trop".into());}
                match index {
                    0 if track.index00.replace(time).is_none() => {},
                    1 if track.index01.replace(time).is_none() => {},
                    0|1=>return Err(format!("INDEX {index:02} dupliqué")),
                    _=>{}, // INDEX 02+ is irrelevant to the first data track.
                }
            },
            _=>return Err(format!("Commande CUE non reconnue : {keyword} (ligne {})",line_number+1)),
        }
    }
    if tracks.is_empty(){return Err("CUE sans pistes TRACK".into());}
    if tracks.iter().any(|t|t.index01.is_none()) {
        return Err("CUE incomplet : INDEX 01 absent".into());
    }
    for t in &tracks {
        if let Some(i0)=t.index00 {
            if i0>t.index01.unwrap() {return Err(format!("TRACK {:02}: INDEX 00 après INDEX 01",t.number));}
        }
    }
    Ok(tracks)
}
/// Resolve CUE references only inside the cue's directory (or descendants).
/// Reject absolute paths, "..", drive paths and symlinks in all path segments.
fn resolve_bin(cue:&Path, filename:&str)->Result<PathBuf,String>{
    let normalized=filename.replace('\\',"/");
    let rel=Path::new(&normalized);
    if rel.is_absolute() || normalized.contains(':') {return Err("CUE : chemin BIN absolu interdit".into());}
    let mut current=cue.parent().ok_or("Chemin CUE sans dossier")?.to_path_buf();
    for part in rel.components(){
        match part{
            Component::Normal(name)=>{
                let candidate=current.join(name);
                // A case-insensitive lookup matches Windows game images on Linux
                // without allowing path escape or ambiguous case-fold collisions.
                let found=if candidate.exists() || candidate.is_symlink() {candidate}
                else{
                    let matches:Vec<_>=fs::read_dir(&current)
                        .map_err(|e|format!("Dossier BIN inaccessible : {e}"))?
                        .filter_map(Result::ok)
                        .filter(|e|e.file_name().to_string_lossy().eq_ignore_ascii_case(&name.to_string_lossy()))
                        .map(|e|e.path()).collect();
                    if matches.len()!=1 {return Err(format!("Fichier BIN absent ou ambigu : {}",normalized));}
                    matches[0].clone()
                };
                let meta=fs::symlink_metadata(&found).map_err(|e|format!("BIN inaccessible : {e}"))?;
                if meta.file_type().is_symlink(){return Err("CUE : liens symboliques refusés".into());}
                current=found;
            },
            Component::CurDir=>{},
            _=>return Err("CUE : chemin BIN non sûr".into()),
        }
    }
    if !current.is_file(){return Err(format!("Fichier BIN absent : {normalized}"));}
    Ok(current)
}
fn data_track(cue:&Path,bin_override:Option<&Path>)->Result<DataTrack,String>{
    let meta=fs::symlink_metadata(cue).map_err(|e|format!("CUE illisible : {e}"))?;
    if !meta.is_file() || meta.file_type().is_symlink() || meta.len()>MAX_CUE_BYTES{
        return Err("CUE non régulier ou trop grand".into());
    }
    let raw=fs::read(cue).map_err(|e|format!("Lecture CUE : {e}"))?;
    let contents=String::from_utf8(raw).map_err(|_|"CUE : texte UTF-8 requis".to_string())?;
    let tracks=parse_cue(&contents)?;
    let first=&tracks[0];
    if !matches!(first.mode,TrackMode::Mode1_2048|TrackMode::Mode1_2352|TrackMode::Mode2_2352) {
        return Err("CUE : première piste doit être une piste de données MODE1/2048, MODE1/2352 ou MODE2/2352".into());
    }
    if first.file_type!="BINARY" {return Err("CUE : fichier de données FILE BINARY requis".into());}
    if tracks.iter().skip(1).any(|t|t.mode!=TrackMode::Audio){
        return Err("CUE multisession ou plusieurs pistes de données non pris en charge".into());
    }
    let file=if let Some(explicit)=bin_override {
        // Explicit positional BIN is allowed outside the CUE directory,
        // but must match the BIN basename declared by its first FILE.
        let declared=first.file.replace('\\',"/");
        let expected_name=Path::new(&declared).file_name().ok_or("Nom de BIN CUE absent")?;
        if !explicit.file_name().is_some_and(|actual|actual.to_string_lossy()
            .eq_ignore_ascii_case(&expected_name.to_string_lossy())){
            return Err("Le BIN explicite ne correspond pas au FILE déclaré dans le CUE".into());
        }
        let metadata=fs::symlink_metadata(explicit).map_err(|e|format!("BIN explicite absent : {e}"))?;
        if !metadata.is_file() || metadata.file_type().is_symlink() {
            return Err("Le BIN explicite doit être un fichier régulier non symbolique".into());
        }
        explicit.to_path_buf()
    }else{resolve_bin(cue,&first.file)?};
    let size=fs::metadata(&file).map_err(|e|e.to_string())?.len();
    if size<17*first.mode.sector_bytes() as u64 || size>MAX_IMAGE_BYTES{
        return Err("BIN trop petit ou trop volumineux".into());
    }
    let frame_bytes=first.mode.sector_bytes() as u64;
    if size%frame_bytes!=0 {return Err("BIN non aligné sur des secteurs CD".into());}
    let first_sector=first.index01.unwrap();
    let mut end_sector=size/frame_bytes;
    for next in tracks.iter().skip(1) {
        if next.file==first.file {
            // A next track within the same BIN starts at INDEX 00 if present
            // (pregap stored in the BIN), otherwise INDEX 01.
            let begin=next.index00.unwrap_or(next.index01.unwrap());
            if begin<first_sector {return Err("CUE : pistes non ordonnées".into());}
            end_sector=end_sector.min(begin);
            break;
        }
    }
    if end_sector<=first_sector+16 {return Err("CUE : piste de données trop courte".into());}
    let sector_count=end_sector-first_sector;
    if sector_count.checked_mul(SECTOR_DATA as u64).is_none_or(|n|n>MAX_IMAGE_BYTES){
        return Err("CUE : données trop volumineuses".into());
    }
    Ok(DataTrack{file,mode:first.mode,first_sector,sector_count})
}
fn sector_data<'a>(sector:&'a [u8], mode:TrackMode)->Result<&'a [u8],String>{
    if mode==TrackMode::Mode1_2048 {return Ok(&sector[..SECTOR_DATA]);}
    if sector.len()!=RAW_SECTOR || sector[..12]!=SYNC {
        return Err("BIN : synchronisation des secteurs CD incorrecte".into());
    }
    match mode{
        TrackMode::Mode1_2352 if sector[15]==1 =>{},
        TrackMode::Mode2_2352 if sector[15]==2 &&
            sector[16..20]==sector[20..24] && sector[18]&0x20==0=>{},
        _=>return Err("BIN : secteur de données incorrect ou Mode2 Form2 non supporté".into()),
    }
    let offset=mode.user_offset();
    Ok(&sector[offset..offset+SECTOR_DATA])
}
pub struct TemporaryIso { pub path: PathBuf }
impl Drop for TemporaryIso {
    fn drop(&mut self) {let _=fs::remove_file(&self.path);}
}
/// Normalize only the first data track into an ephemeral 2048-byte-sector ISO.
/// The existing isomage filesystem parser then verifies actual original game
/// hashes; the normalized file is always removed after installation.
pub fn normalize_cue(
    cue:&Path,output_dir:&Path,progress:impl FnMut(u64,u64,&str),
    log:impl FnMut(&str),
)->Result<TemporaryIso,String>{
    normalize_cue_with_bin(cue,None,output_dir,progress,log)
}
pub fn normalize_cue_with_bin(
    cue:&Path,bin_override:Option<&Path>,output_dir:&Path,
    mut progress:impl FnMut(u64,u64,&str),
    mut log:impl FnMut(&str),
)->Result<TemporaryIso,String>{
    let data=data_track(cue,bin_override)?;
    log(&format!("CUE: mode {:?}, sectors {}, first INDEX 01={}, source {}",
        data.mode,data.sector_count,data.first_sector,data.file.display()));
    let mut source=File::open(&data.file).map_err(|e|format!("BIN illisible : {e}"))?;
    let start=data.first_sector*data.mode.sector_bytes() as u64;
    source.seek(SeekFrom::Start(start)).map_err(|e|e.to_string())?;
    let mut target:Option<(TemporaryIso,File)>=None;
    for nonce in 0..1000 {
        // Deliberately hidden: image discovery must never select this ISO.
        let path=output_dir.join(format!(".scorchedplanetfix-cue-{}-{nonce}.iso",std::process::id()));
        match OpenOptions::new().write(true).create_new(true).open(&path) {
            Ok(f)=>{target=Some((TemporaryIso{path},f));break;},
            Err(e) if e.kind()==std::io::ErrorKind::AlreadyExists=>continue,
            Err(e)=>return Err(format!("CUE : création ISO temporaire impossible : {e}")),
        }
    }
    let (temporary,mut dest)=target.ok_or("CUE : impossible de créer un ISO temporaire")?;
    let mut sector=vec![0u8;data.mode.sector_bytes()];
    let mut copied=0u64;
    for index in 0..data.sector_count {
        source.read_exact(&mut sector).map_err(|e|format!("BIN tronqué (secteur {index}) : {e}"))?;
        let user=sector_data(&sector,data.mode)?;
        if index==16 && (user[0]!=1 || &user[1..6]!=b"CD001" || user[6]!=1){
            return Err("CUE/BIN : volume ISO9660 principal CD001 absent (piste incorrecte ?)".into());
        }
        dest.write_all(user).map_err(|e|format!("ISO temporaire : {e}"))?;
        copied+=1;
        if copied%1024==0 || copied==data.sector_count {
            progress(copied,data.sector_count,"Conversion de la piste CUE/BIN…");
        }
    }
    dest.flush().map_err(|e|e.to_string())?;
    dest.sync_all().map_err(|e|e.to_string())?;
    log(&format!("CUE: ISO temporaire créé ({} octets, piste de données uniquement)",
        copied*SECTOR_DATA as u64));
    Ok(temporary)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn parse_msf_and_cue_tracks(){
        assert_eq!(msf_frames("01:00:00").unwrap(),4500);
        assert_eq!(msf_frames("00:02:00").unwrap(),150);
        assert!(msf_frames("01:60:00").is_err());
        assert!(msf_frames("00:00:75").is_err());
        let cue="FILE \"My disc.bin\" BINARY\r\n TRACK 01 MODE1/2352\r\n INDEX 01 00:00:00\r\n TRACK 02 AUDIO\r\n INDEX 00 44:30:00\r\n INDEX 01 44:32:00\r\n";
        let t=parse_cue(cue).unwrap();
        assert_eq!(t.len(),2);
        assert_eq!(t[0].mode,TrackMode::Mode1_2352);
        assert_eq!(t[1].index00,Some(44*60*75+30*75));
    }
    #[test] fn explicit_bin_match_and_missing_is_rejected(){
        let root=std::env::temp_dir().join(format!("scorched-explicit-bin-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let cue=root.join("game.cue");
        fs::write(&cue,b"FILE \"disc.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n").unwrap();
        fs::write(root.join("wrong.bin"),vec![0u8;20*2352]).unwrap();
        assert!(data_track(&cue,Some(&root.join("wrong.bin"))).is_err());
        let good=root.join("disc.bin");
        fs::write(&good,vec![0u8;20*2352]).unwrap();
        assert!(data_track(&cue,Some(&good)).is_ok());
        fs::remove_dir_all(root).unwrap();
    }
    #[test] fn rejects_unsafe_cues(){
        assert!(parse_cue("TRACK 01 MODE1/2352\n INDEX 01 00:00:00").is_err());
        assert!(parse_cue("FILE \"x.bin\" BINARY\nTRACK 02 MODE1/2352\nINDEX 01 00:00:00").is_err());
        assert!(parse_cue("FILE \"x.bin\" BINARY\nTRACK 01 MODE1/2352").is_err());
        assert!(parse_cue("FILE \"x.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nINDEX 01 00:00:01").is_err());
    }
    #[test] fn raw_sector_slices_and_rejects_wrong_mode(){
        let mut s=vec![0;2352];
        s[..12].copy_from_slice(&SYNC);
        s[15]=1;
        s[16..16+2048].fill(0x5a);
        assert_eq!(sector_data(&s,TrackMode::Mode1_2352).unwrap(),&vec![0x5a;2048]);
        assert!(sector_data(&s,TrackMode::Mode2_2352).is_err());
        s[15]=2;
        s[16..24].fill(0);
        s[24..24+2048].fill(0xa5);
        assert_eq!(sector_data(&s,TrackMode::Mode2_2352).unwrap(),&vec![0xa5;2048]);
        s[18]=0x20; s[22]=0x20; // XA Form 2 is not a 2048-byte filesystem sector
        assert!(sector_data(&s,TrackMode::Mode2_2352).is_err());
    }
    #[test] fn normalize_synthetic_raw_cd_with_audio_in_same_bin(){
        let root=std::env::temp_dir().join(format!("scorched-cue-{}-{}",std::process::id(),42));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let cue=root.join("sample.cue");
        let bin=root.join("sample.bin");
        let mut content=vec![0u8;20*2352];
        for i in 0..20 {
            let o=i*2352;
            content[o..o+12].copy_from_slice(&SYNC);
            content[o+15]=1;
        }
        let sector16=16*2352+16;
        content[sector16]=1;
        content[sector16+1..sector16+6].copy_from_slice(b"CD001");
        content[sector16+6]=1;
        fs::write(&bin,&content).unwrap();
        fs::write(&cue,b"FILE \"sample.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n TRACK 02 AUDIO\n INDEX 01 00:00:20\n").unwrap();
        let t=normalize_cue(&cue,&root,|_,_,_|{},|_|{}).unwrap();
        assert_eq!(fs::metadata(&t.path).unwrap().len(),20*2048);
        let mut d=vec![0;20*2048];
        File::open(&t.path).unwrap().read_exact(&mut d).unwrap();
        assert_eq!(&d[16*2048+1..16*2048+6],b"CD001");
        let temporary=t.path.clone();
        drop(t);
        assert!(!temporary.exists());
        fs::remove_dir_all(root).unwrap();
    }
}