// SPDX-License-Identifier: GPL-3.0-or-later
//! ISO9660/Joliet (or UDF) game install into a *new* splanet directory.
//!
//! Preflight: parse image, reject unsafe/ambiguous paths, require exact known
//! 3dfx exe+driver hashes, required game resources and at least one SMK video (INI generated if absent).
//! Extract: flatten SPLANET then VOODOO (VOODOO wins), verify every extracted
//! file's SHA256 against bytes read from ISO, update video= to local install.
//! Never delete the user's ISO, existing install, or overwrite a destination.
use scorched_planet_fix::ORIGINAL_SHA256;
use isomage::{cat_node, detect_and_parse_filesystem, TreeNode};
use sha2::{Digest, Sha256};
use std::collections::BTreeMap;
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

const DRIVER_SHA256: &str = "1bd95e0dfeaaa47c53d5e2d4206027968790b928949321efd1eb2bb70f9201de";
const MAX_ENTRIES: usize = 25_000;
const MAX_TOTAL_BYTES: u64 = 8 * 1024 * 1024 * 1024;

#[derive(Clone)]
struct Asset { node: TreeNode, relative: PathBuf, size: u64, from_voodoo: bool }
#[derive(Default)]
struct Inventory { files: BTreeMap<String, Asset>, image_entries: usize, video_files: usize }
pub struct InstallReport { pub files: usize, pub bytes: u64, pub folder: PathBuf }
fn err(msg: impl ToString) -> String { msg.to_string() }

fn valid_name(name: &str) -> bool {
    !name.is_empty() && !matches!(name, "." | "..")
        && !name.starts_with(' ') && !name.ends_with([' ', '.'])
        && !name.contains(['/', '\\', ':', '\0'])
        && !name.chars().any(|c| c.is_control())
        && !matches!(name.to_ascii_uppercase().split('.').next().unwrap_or(""),
            "CON" | "PRN" | "AUX" | "NUL" | "COM1" | "COM2" | "COM3" |
            "COM4" | "COM5" | "COM6" | "COM7" | "COM8" | "COM9" |
            "LPT1" | "LPT2" | "LPT3" | "LPT4" | "LPT5" |
            "LPT6" | "LPT7" | "LPT8" | "LPT9")
}
fn collect(node: &TreeNode, prefix: &Path, voodoo: bool, inventory: &mut Inventory)
    -> Result<(), String> {
    for child in &node.children {
        inventory.image_entries += 1;
        if inventory.image_entries > MAX_ENTRIES {
            return Err(err("ISO non prise en charge (nombre d'entrées excessif)"));
        }
        if !valid_name(&child.name) {
            return Err(err("ISO non prise en charge (nom de fichier dangereux)"));
        }
        let relative = prefix.join(&child.name);
        if child.is_directory {
            collect(child, &relative, voodoo, inventory)?;
            continue;
        }
        if child.file_length != Some(child.size) || child.file_location.is_none() {
            return Err(format!("ISO non prise en charge (extent absent : {})",relative.display()));
        }
        if child.size > MAX_TOTAL_BYTES {
            return Err(err("ISO non prise en charge (fichier trop grand)"));
        }
        let key = relative.to_string_lossy().replace('\\', "/").to_ascii_lowercase();
        if !voodoo && key.starts_with("video/") && key.ends_with(".smk") {
            inventory.video_files += 1;
        }
        if let Some(previous) = inventory.files.get(&key) {
            // Identical paths within a source tree are ambiguous, even on
            // case-insensitive Windows. Only VOODOO is allowed to override SPLANET.
            if previous.from_voodoo == voodoo {
                return Err(format!("ISO non prise en charge (doublon : {key})"));
            }
        }
        inventory.files.insert(key, Asset {
            node: child.clone(), relative, size: child.size, from_voodoo: voodoo,
        });
    }
    Ok(())
}
fn find_dir<'a>(parent: &'a TreeNode, name: &str) -> Result<&'a TreeNode, String> {
    let matches: Vec<_> = parent.children.iter().filter(|n|
        n.is_directory && n.name.eq_ignore_ascii_case(name)).collect();
    match matches.as_slice() {
        [one] => Ok(one),
        _ => Err(format!("Image du jeu non supportée : répertoire {name} absent ou ambigu")),
    }
}
fn inventory(root: &TreeNode) -> Result<Inventory, String> {
    let mut list = Inventory::default();
    collect(find_dir(root, "SPLANET")?, Path::new(""), false, &mut list)?;
    collect(find_dir(root, "VOODOO")?, Path::new(""), true, &mut list)?;
    for item in ["spvoodoo.exe", "rwdlvood.dll", "geometry.rwb"] {
        if !list.files.contains_key(item) {
            return Err(format!("Image du jeu non supportée : fichier {item} absent"));
        }
    }
    if list.video_files == 0 {
        return Err(err("Image du jeu non supportée : vidéos SMK absentes"));
    }
    if !list.files["spvoodoo.exe"].from_voodoo || !list.files["rwdlvood.dll"].from_voodoo {
        return Err(err("Image du jeu non supportée : composants VOODOO absents"));
    }
    let bytes: u64 = list.files.values().map(|a| a.size).sum();
    if bytes == 0 || bytes > MAX_TOTAL_BYTES {
        return Err(err("Image du jeu non supportée : taille des données invalide"));
    }
    Ok(list)
}

struct HashWriter<'a, W: Write, F: FnMut(u64)> {
    output: &'a mut W, hasher: Sha256, transferred: u64, report: F,
}
impl<W: Write, F: FnMut(u64)> Write for HashWriter<'_, W, F> {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        let n=self.output.write(buf)?;
        self.hasher.update(&buf[..n]);
        self.transferred += n as u64;
        (self.report)(n as u64);
        Ok(n)
    }
    fn flush(&mut self) -> std::io::Result<()> { self.output.flush() }
}
fn node_digest(iso: &mut File, node: &TreeNode) -> Result<String, String> {
    let mut sink=std::io::sink();
    let mut writer=HashWriter { output: &mut sink, hasher: Sha256::new(),
        transferred: 0, report: |_| {} };
    cat_node(iso,node,&mut writer).map_err(|e|format!("Lecture ISO : {e}"))?;
    if writer.transferred != node.size {
        return Err("Image du jeu non supportée : fichier ISO tronqué".into());
    }
    Ok(format!("{:x}",writer.hasher.finalize()))
}
fn file_digest(path: &Path) -> Result<String, String> {
    let mut file=File::open(path).map_err(|e|e.to_string())?;
    let mut hasher=Sha256::new();
    let mut buf=[0u8;64*1024];
    loop {
        let count=file.read(&mut buf).map_err(|e|e.to_string())?;
        if count==0 {break;}
        hasher.update(&buf[..count]);
    }
    Ok(format!("{:x}",hasher.finalize()))
}
fn validate_bounds(iso_len: u64, root: &TreeNode) -> Result<(),String> {
    fn walk(node: &TreeNode, length: u64, count: &mut usize)->Result<(),String>{
        *count += 1;
        if *count > MAX_ENTRIES {return Err("Image non supportée : trop d'entrées".into());}
        if !node.is_directory {
            let start=node.file_location.ok_or("Image non supportée : extent manquant")?;
            let size=node.file_length.ok_or("Image non supportée : taille manquante")?;
            if start.checked_add(size).is_none_or(|end|end>length) || size!=node.size {
                return Err("Image non supportée : extent hors ISO ou fichier tronqué".into());
            }
        }
        for child in &node.children {
            if !valid_name(&child.name) {
                return Err("Image non supportée : nom incorrect".into());
            }
            walk(child,length,count)?;
        }
        Ok(())
    }
    let mut count=0; walk(root,iso_len,&mut count)
}
fn mandatory_file_hashes(iso: &mut File, files: &Inventory) -> Result<(),String>{
    for (name,expected) in [
        ("spvoodoo.exe", ORIGINAL_SHA256),
        ("rwdlvood.dll", DRIVER_SHA256),
    ] {
        let actual=node_digest(iso,&files.files[name].node)?;
        if actual!=expected {
            return Err(format!("Image du jeu non supportée : variante ou fichier altéré ({name}, SHA-256 {actual})"));
        }
    }
    Ok(())
}
pub fn find_image_and_install(folder: &Path) -> Result<Option<PathBuf>,String> {
    let mut images = Vec::new();
    let entries=fs::read_dir(folder).map_err(|e|e.to_string())?;
    for item in entries {
        let item=item.map_err(|e|e.to_string())?;
        let ty=item.file_type().map_err(|e|e.to_string())?;
        if ty.is_symlink() {continue;}
        if ty.is_file() && item.path().extension().is_some_and(|ext|ext.eq_ignore_ascii_case("iso")){
            images.push(item.path());
        }
    }
    images.sort();
    if images.len()>1 {return Err("Plusieurs ISO trouvées : laisser une seule image du jeu".into());}
    Ok(images.pop())
}
fn update_ini(ini:&Path,video:&Path)->Result<(),String>{
    // Preserve all legacy non-UTF8 bytes in original INI lines; replace
    // video= as ASCII, add missing 3dfx defaults from the user's Lutris
    // installation script without overwriting existing user settings.
    let original=match fs::read(ini) {
        Ok(bytes)=>bytes,
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>Vec::new(),
        Err(e)=>return Err(format!("Lecture splanet.ini : {e}")),
    };
    let names: [&[u8];4]=[b"video=",b"HighQualityVideo=",b"settings=",b"name="];
    let values: [Vec<u8>;4]=[
        format!("video={}",video.display()).into_bytes(),
        b"HighQualityVideo=1".to_vec(),
        b"settings=1 0 1 1 5 95 127 0 0 5 320".to_vec(),
        b"name=Player".to_vec(),
    ];
    let mut out:Vec<Vec<u8>>=Vec::new();
    let mut in_scorch=false;
    let mut section_found=false;
    let mut seen=[false;4];
    let chunks:Vec<&[u8]>=original.split(|b|*b==b'\n').collect();
    for (index,line) in chunks.iter().enumerate() {
        if index+1==chunks.len() && line.is_empty() {continue;}
        let line=line.strip_suffix(b"\r").unwrap_or(line);
        let trimmed=line.trim_ascii();
        if trimmed.starts_with(b"[") && trimmed.ends_with(b"]") {
            if in_scorch {
                for i in 0..4 {
                    if !seen[i] {out.push(values[i].clone());seen[i]=true;}
                }
            }
            in_scorch=trimmed.eq_ignore_ascii_case(b"[scorch]");
            section_found |= in_scorch;
        }
        if in_scorch {
            let mut handled=false;
            for i in 0..4 {
                if trimmed.len()>=names[i].len() &&
                   trimmed[..names[i].len()].eq_ignore_ascii_case(names[i]) {
                    if i==0 {
                        out.push(values[0].clone());
                    }else{
                        out.push(line.to_vec());
                    }
                    seen[i]=true;
                    handled=true;
                    break;
                }
            }
            if handled {continue;}
        }
        out.push(line.to_vec());
    }
    if !section_found {
        out.push(b"[scorch]".to_vec());
        in_scorch=true;
    }
    if in_scorch {
        for i in 0..4 {if !seen[i]{out.push(values[i].clone());}}
    }
    let mut bytes=Vec::new();
    for line in out {
        bytes.extend_from_slice(&line);
        bytes.extend_from_slice(b"\r\n");
    }
    fs::write(ini,bytes).map_err(|e|format!("INI : {e}"))
}

pub fn install_iso(
    image:&Path, destination:&Path,
    mut report:impl FnMut(u64,u64,&str),
    mut log:impl FnMut(&str),
) -> Result<InstallReport,String> {
    if destination.exists() || destination.is_symlink(){
        return Err("Installation existante splanet : refus d'écrasement".into());
    }
    let mut iso=File::open(image).map_err(|e|format!("ISO illisible : {e}"))?;
    let iso_len=iso.metadata().map_err(|e|e.to_string())?.len();
    if iso_len < 32 * 2048 || iso_len > MAX_TOTAL_BYTES {
        return Err("Image du jeu non supportée : taille ISO incorrecte".into());
    }
    report(0,100,"Analyse de l'image du CD…");
    log("ISO: parsing read-only image filesystem");
    let root=detect_and_parse_filesystem(&mut iso,&image.to_string_lossy())
        .map_err(|e|format!("Image du jeu non supportée : {e}"))?;
    validate_bounds(iso_len,&root)?;
    let inventory=inventory(&root)?;
    mandatory_file_hashes(&mut iso,&inventory)?;
    // Signature checks prove the supported 3dfx binary edition. Without a
    // signed full-disc reference manifest, they cannot prove that every
    // other original CD file matches a specific retail master image.
    log("ISO: required original EXE and RWDLVOOD hashes verified; no trusted all-files master manifest yet");
    log(&format!("ISO: {} install files; {} SMK videos", inventory.files.len(),inventory.video_files));
    let parent=destination.parent().ok_or("Invalid installation parent")?;
    let staging=parent.join(format!(".splanet-setup-{}",std::process::id()));
    fs::create_dir(&staging).map_err(|e|format!("Can't stage installation: {e}"))?;
    let install_result=(||{
        let total: u64=inventory.files.values().map(|a|a.size).sum();
        let mut done=0u64;
        for (key,asset) in &inventory.files {
            let output=staging.join(&asset.relative);
            if let Some(dir)=output.parent(){fs::create_dir_all(dir).map_err(|e|e.to_string())?;}
            let mut file=OpenOptions::new().write(true).create_new(true).open(&output)
                .map_err(|e|format!("Création {} : {e}",asset.relative.display()))?;
            let mut writer=HashWriter{output:&mut file,hasher:Sha256::new(),
                transferred:0,report:|delta| {
                    done+=delta;
                    report(10+done.saturating_mul(85)/total,100,"Extraction des fichiers du jeu…");
                }};
            cat_node(&mut iso,&asset.node,&mut writer)
                .map_err(|e|format!("Extraction {key} : {e}"))?;
            if writer.transferred != asset.size {
                return Err(format!("Données incomplètes : {key}"));
            }
            let content_hash=format!("{:x}",writer.hasher.finalize());
            file.flush().map_err(|e|e.to_string())?;
            file.sync_all().map_err(|e|e.to_string())?;
            drop(file);
            let copied_hash=file_digest(&output)?;
            if copied_hash!=content_hash {
                return Err(format!("Echec de vérification des données extraites : {key}"));
            }
            // Make original CD-ROM read-only files writable after copying.
            let mut permission=fs::metadata(&output).map_err(|e|e.to_string())?.permissions();
            permission.set_readonly(false);
            fs::set_permissions(&output,permission).map_err(|e|e.to_string())?;
            log(&format!("EXTRACT: {key} bytes={} sha256={content_hash}",asset.size));
        }
        report(97,100,"Configuration de splanet.ini…");
        // The actual installed folder is renamed from staging after success:
        // write the FINAL destination path, not the staging path.
        let ini=staging.join("splanet.ini");
        let ini=if ini.is_file(){ini}else {
            let candidates=fs::read_dir(&staging).map_err(|e|e.to_string())?;
            candidates.filter_map(Result::ok).map(|e|e.path()).find(|p|
                p.file_name().is_some_and(|n|n.to_string_lossy().eq_ignore_ascii_case("splanet.ini"))
            ).unwrap_or_else(||staging.join("splanet.ini"))
        };
        let generated_ini=!ini.exists();
        if generated_ini {
            log("INI: absent from original CD; generating [scorch] configuration");
        } else {
            log("INI: preserving original settings; updating local video path");
        }
        update_ini(&ini,&destination.join("video"))?;
        log(&format!("INI: written and verified: {}",file_digest(&ini)?));
        // User-specific resources and wrappers are NOT embedded or downloaded.
        if destination.exists(){return Err("splanet créé entre-temps : opération annulée".into());}
        fs::rename(&staging,destination).map_err(|e|format!("Finalisation de l'installation : {e}"))?;
        report(100,100,"Installation terminée");
        let file_count=inventory.files.len()+usize::from(generated_ini);
        log(&format!("INSTALL-OK: {} files ({} from CD, {} generated), {} extracted bytes",
            file_count,inventory.files.len(),usize::from(generated_ini),total));
        Ok(InstallReport{files:file_count,bytes:total,folder:destination.to_owned()})
    })();
    if install_result.is_err(){
        let _=fs::remove_dir_all(&staging);
        log("INSTALL-ERROR: extraction aborted; staging removed, original ISO unchanged");
    }
    install_result
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn reject_filename_traversal_and_reserved(){
        for n in ["..",".","", "a/b","a\\b","C:foo","CON","NUL.txt"," test."," bad\n"]{
            assert!(!valid_name(n),"{n:?}");
        }
        assert!(valid_name("SPVOODOO.EXE"));
    }
    #[test] fn disc_data_must_include_videos_and_exact_driver(){
        let mut root=TreeNode::new_directory("/".into());
        let mut sp=TreeNode::new_directory("SPLANET".into());
        sp.add_child(TreeNode::new_file_with_location("GEOMETRY.RWB".into(),10,2048,10));
        let mut video=TreeNode::new_directory("VIDEO".into());
        video.add_child(TreeNode::new_file_with_location("INTRO.SMK".into(),10,4096,10));
        sp.add_child(video);
        let mut voodoo=TreeNode::new_directory("VOODOO".into());
        voodoo.add_child(TreeNode::new_file_with_location("SPVOODOO.EXE".into(),10,8192,10));
        voodoo.add_child(TreeNode::new_file_with_location("RWDLVOOD.DLL".into(),10,12288,10));
        root.add_child(sp);
        root.add_child(voodoo);
        let checked=inventory(&root).expect("CD sans splanet.ini doit être accepté");
        assert!(!checked.files.contains_key("splanet.ini"));
        assert_eq!(checked.video_files,1);
    }
    #[test] fn creates_missing_ini_with_local_video_and_defaults(){
        let dir=std::env::temp_dir().join(format!("scorched-new-ini-test-{}",std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let ini=dir.join("splanet.ini");
        assert!(!ini.exists());
        update_ini(&ini,Path::new(r"C:\splanet\video")).unwrap();
        let result=fs::read_to_string(&ini).unwrap();
        assert!(result.contains("[scorch]"));
        assert!(result.contains(r"video=C:\splanet\video"));
        assert!(result.contains("HighQualityVideo=1"));
        assert!(result.contains("settings=1 0 1 1 5 95 127 0 0 5 320"));
        assert!(result.contains("name=Player"));
        fs::remove_dir_all(&dir).unwrap();
    }
    #[test] fn ini_video_path_is_local(){
        let dir=std::env::temp_dir().join(format!("scorched-ini-test-{}",std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let ini=dir.join("splanet.ini");
        fs::write(&ini,b"[scorch]\r\nHighQualityVideo=1\r\nvideo=c:\\splanetcd\\splanet\\video\r\n").unwrap();
        update_ini(&ini,Path::new(r"C:\splanet\video")).unwrap();
        let result=fs::read_to_string(&ini).unwrap();
        assert!(result.contains(r"video=C:\splanet\video"));
        assert!(!result.contains("splanetcd"));
        fs::remove_dir_all(&dir).unwrap();
    }
}
