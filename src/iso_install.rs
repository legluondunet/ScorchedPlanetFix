// SPDX-License-Identifier: GPL-3.0-or-later
//! ISO9660/Joliet (or UDF) game install into a *new* splanet directory.
//!
//! Preflight: parse image, reject unsafe/ambiguous paths, require exact known
//! 3dfx exe+driver hashes, required game resources and at least one SMK video (INI generated if absent).
//! Extract: unpack the two necessary CD trees into temp/splanet and temp/voodoo,
//! copy temp/splanet to staged splanet (canonical lowercase names), then copy
//! the two playable VOODOO game components, excluding obsolete 3dfx drivers.
//! Verify SHA-256 for both extraction and staged copies, patch before writing
//! the final INI, then atomically publish the complete installation.
//! Never delete the user's ISO, existing install, or overwrite a destination.
use scorched_planet_fix::{self,Action,ORIGINAL_SHA256};
use crate::cue_image;
use isomage::{cat_node, detect_and_parse_filesystem, TreeNode};
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

const DRIVER_SHA256: &str = "1bd95e0dfeaaa47c53d5e2d4206027968790b928949321efd1eb2bb70f9201de";
const MAX_ENTRIES: usize = 25_000;
const MAX_TOTAL_BYTES: u64 = 8 * 1024 * 1024 * 1024;

#[derive(Clone)]
struct Asset { node: TreeNode, relative: PathBuf, size: u64, from_voodoo: bool }
#[derive(Default)]
struct Inventory {
    files: BTreeMap<String, Asset>,
    splanet_files: BTreeMap<String, Asset>,
    voodoo_files: BTreeMap<String, Asset>,
    seen_splanet: BTreeSet<String>,
    seen_voodoo: BTreeSet<String>,
    image_entries: usize, video_files: usize, skipped_legacy_drivers: usize,
}
pub struct InstallReport {
    pub files: usize, pub bytes: u64, pub folder: PathBuf,
    pub edition: &'static str, pub patch_report: String,
}

/// Fingerprints recorded from the three owner-supplied CD file inventories.
/// This is not a third-party authenticated retail-master database.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Edition {
    label: &'static str,
    installer_key: &'static str,
    exe_sha256: &'static str,
    geometry_sha256: &'static str,
}
const EDITIONS: [Edition; 3] = [
    Edition { label: "US", installer_key: "1.08",
        exe_sha256: ORIGINAL_SHA256,
        geometry_sha256: "85e153739a5c43e514e2a8146d0cdfccb2c998d73e1494cfd45e1aa05f11531e" },
    Edition { label: "EN/ES/IT", installer_key: "1.06",
        exe_sha256: "9ab6442553d055a14273ee0e3c5a912986662dc0a3cc8512ac716879b5068e5f",
        geometry_sha256: "29b3d8f87eed27ca933b81231b84f7e406506b498e5fc74bfee52f5b1004fb1b" },
    Edition { label: "FR/DE", installer_key: "1.06",
        exe_sha256: "eff38cedf2408566060b3c7cf39be59c54d23e67b0a4c7cb0725051aa87e1b72",
        geometry_sha256: "a2dc1882d66f0d7607a955c8bf9ad28282c3d1d08ca0675a90ca93ecd20f6c69" },
];

fn identify_edition(exe_hash: &str, geometry_hash: &str) -> Option<Edition> {
    EDITIONS.iter().copied().find(|edition|
        edition.exe_sha256 == exe_hash && edition.geometry_sha256 == geometry_hash)
}
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
        // Preserve source names inside the ISO, but normalize installation
        // paths to ASCII lowercase: all three inventoried editions use ASCII
        // filesystem names. Never allow a case-fold collision, including a
        // directory with a file of the same normalized name.
        let relative=prefix.join(child.name.to_ascii_lowercase());
        let key=relative.to_string_lossy().replace('\\', "/");
        let seen=if voodoo {&mut inventory.seen_voodoo}else{&mut inventory.seen_splanet};
        if !seen.insert(key.clone()) {
            return Err(format!("Image ambiguë : fichiers/répertoires de casse différente ({key})"));
        }
        if child.is_directory {
            collect(child, &relative, voodoo, inventory)?;
            continue;
        }
        // Older drivers in European CDs are not needed on modern Wine/Windows.
        // Drop all three only at the root of the original VOODOO directory.
        if voodoo && prefix.as_os_str().is_empty() &&
            matches!(key.as_str(),"glide.dll" | "fxmemmap.vxd" | "sst1init.dll") {
            inventory.skipped_legacy_drivers += 1;
            continue;
        }
        if child.file_length != Some(child.size) || child.file_location.is_none() {
            return Err(format!("ISO non prise en charge (extent absent : {})",relative.display()));
        }
        if child.size > MAX_TOTAL_BYTES {
            return Err(err("ISO non prise en charge (fichier trop grand)"));
        }
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
        let asset=Asset {node:child.clone(),relative,size:child.size,from_voodoo:voodoo};
        if voodoo {
            inventory.voodoo_files.insert(key.clone(),asset.clone());
        }else{
            inventory.splanet_files.insert(key.clone(),asset.clone());
        }
        inventory.files.insert(key, asset);
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
    // No overlap is expected in the three known original media inventories.
    // An unexpected VOODOO override would risk silently replacing game data.
    if list.voodoo_files.keys().any(|key|list.splanet_files.contains_key(key)) {
        return Err("Image du jeu ambiguë : fichiers SPLANET/VOODOO en conflit".into());
    }
    // Reject file/directory collisions across both source trees.
    if list.voodoo_files.keys().any(|key|list.seen_splanet.contains(key)) ||
       list.splanet_files.keys().any(|key|list.seen_voodoo.contains(key)) {
        return Err("Image du jeu ambiguë : fichier et répertoire en conflit".into());
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
fn mandatory_file_hashes(iso: &mut File, files: &Inventory) -> Result<Edition,String>{
    let driver = node_digest(iso, &files.files["rwdlvood.dll"].node)?;
    if driver != DRIVER_SHA256 {
        return Err(format!("Image du jeu non supportée : RWDLVOOD.DLL non reconnu (SHA-256 {driver})"));
    }
    let executable = node_digest(iso, &files.files["spvoodoo.exe"].node)?;
    let geometry = node_digest(iso, &files.files["geometry.rwb"].node)?;
    identify_edition(&executable, &geometry).ok_or_else(|| format!(
        "Edition 3dfx inconnue ou fichiers incompatibles : SPVOODOO.EXE={executable}, GEOMETRY.RWB={geometry}"))
}
pub fn find_image_and_install(folder: &Path) -> Result<Option<PathBuf>,String> {
    let mut images = Vec::new();
    let mut bare_bins = 0;
    let entries=fs::read_dir(folder).map_err(|e|e.to_string())?;
    for item in entries {
        let item=item.map_err(|e|e.to_string())?;
        let ty=item.file_type().map_err(|e|e.to_string())?;
        if ty.is_symlink() || !ty.is_file() {continue;}
        if item.file_name().to_string_lossy().starts_with(".scorchedplanetfix-"){continue;}
        let extension=item.path().extension().map(|e|e.to_string_lossy().to_ascii_lowercase());
        match extension.as_deref(){
            Some("iso")|Some("cue")=>images.push(item.path()),
            Some("bin")=>bare_bins+=1,
            _=>{},
        }
    }
    images.sort();
    if images.len()>1 {return Err("Plusieurs images ISO/CUE trouvées : laisser une seule image du jeu".into());}
    if images.is_empty() && bare_bins>0 {
        return Err("Image BIN détectée sans fichier CUE : placez le .cue associé dans ce dossier".into());
    }
    Ok(images.pop())
}
/// Keep all existing user settings unmodified except video, selected language
/// and HighQualityVideo; add conservative defaults when no INI exists.
pub fn update_ini(ini:&Path,video:&Path,language:u8)->Result<(),String>{
    if !(1..=5).contains(&language) {
        return Err(format!("Code de langue invalide: {language}"));
    }
    // Preserve all legacy non-UTF8 bytes in original INI lines; replace
    // video= as ASCII, add missing 3dfx defaults from the user's Lutris
    // installation script without overwriting existing user settings.
    let original=match fs::read(ini) {
        Ok(bytes)=>bytes,
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>Vec::new(),
        Err(e)=>return Err(format!("Lecture splanet.ini : {e}")),
    };
    // Numeric language indices come from the original game's PE32 resource
    // pointer table: 1 English, 2 French, 3 German, 4 Italian, 5 Spanish.
    let names: [&[u8];5]=[b"video=",b"language=",b"HighQualityVideo=",b"settings=",b"name="];
    let values: [Vec<u8>;5]=[
        format!("video={}",video.display()).into_bytes(),
        format!("language={language}").into_bytes(),
        b"HighQualityVideo=1".to_vec(),
        b"settings=1 0 1 1 5 95 127 0 0 5 320".to_vec(),
        b"name=Player".to_vec(),
    ];
    let mut out:Vec<Vec<u8>>=Vec::new();
    let mut in_scorch=false;
    let mut section_found=false;
    let mut seen=[false;5];
    let chunks:Vec<&[u8]>=original.split(|b|*b==b'\n').collect();
    for (index,line) in chunks.iter().enumerate() {
        if index+1==chunks.len() && line.is_empty() {continue;}
        let line=line.strip_suffix(b"\r").unwrap_or(line);
        let trimmed=line.trim_ascii();
        if trimmed.starts_with(b"[") && trimmed.ends_with(b"]") {
            if in_scorch {
                for i in 0..5 {
                    if !seen[i] {out.push(values[i].clone());seen[i]=true;}
                }
            }
            in_scorch=trimmed.eq_ignore_ascii_case(b"[scorch]");
            section_found |= in_scorch;
        }
        if in_scorch {
            let mut handled=false;
            for i in 0..5 {
                if trimmed.len()>=names[i].len() &&
                   trimmed[..names[i].len()].eq_ignore_ascii_case(names[i]) {
                    if i<=2 {
                        out.push(values[i].clone());
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
        for i in 0..5 {if !seen[i]{out.push(values[i].clone());}}
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
    report:impl FnMut(u64,u64,&str),
    log:impl FnMut(&str),
) -> Result<InstallReport,String> {
    install_image(image,None,destination,report,log)
}
pub fn install_image(
    image:&Path, bin_override:Option<&Path>, destination:&Path,
    mut report:impl FnMut(u64,u64,&str),
    mut log:impl FnMut(&str),
) -> Result<InstallReport,String> {
    if destination.exists() || destination.is_symlink(){
        return Err("Installation existante splanet : refus d'écrasement".into());
    }
    // CUE/BIN is normalized to a temporary data-only ISO. The guard deletes
    // it on any success/error path; source .cue and .bin stay read-only.
    let temporary=if image.extension().is_some_and(|e|e.eq_ignore_ascii_case("cue")){
        let parent=destination.parent().ok_or("Dossier de destination invalide")?;
        report(0,100,"Analyse et conversion de l'image CUE/BIN…");
        Some(cue_image::normalize_cue_with_bin(image,bin_override,parent,
            |done,total,label|report(done.saturating_mul(9)/total,100,label),
            |message|log(message))?)
    } else {
        if bin_override.is_some() {return Err("Un BIN explicite nécessite un fichier CUE".into());}
        if !image.extension().is_some_and(|e|e.eq_ignore_ascii_case("iso")){
            return Err("Image non reconnue : préciser un fichier ISO ou CUE".into());
        }
        None
    };
    let source=temporary.as_ref().map(|x|x.path.as_path()).unwrap_or(image);
    let mut iso=File::open(source).map_err(|e|format!("Image du jeu illisible : {e}"))?;
    let iso_len=iso.metadata().map_err(|e|e.to_string())?.len();
    if iso_len < 32 * 2048 || iso_len > MAX_TOTAL_BYTES {
        return Err("Image du jeu non supportée : taille ISO incorrecte".into());
    }
    report(10,100,"Analyse de l'image du CD…");
    log("ISO: parsing read-only image filesystem");
    let root=detect_and_parse_filesystem(&mut iso,&source.to_string_lossy())
        .map_err(|e|format!("Image du jeu non supportée : {e}"))?;
    validate_bounds(iso_len,&root)?;
    let inventory=inventory(&root)?;
    let edition=mandatory_file_hashes(&mut iso,&inventory)?;
    log(&format!("EDITION: {} (installer key {}), exact EXE + GEOMETRY + RWDLVOOD validated",
        edition.label,edition.installer_key));
    // Signature checks identify one of the three recorded 3dfx editions. Without a
    // signed full-disc reference manifest, they cannot prove that every
    // other original CD file matches a specific retail master image.
    log("ISO: edition-specific EXE/GEOMETRY and shared RWDLVOOD hashes verified; no independently authenticated all-files master yet");
    log(&format!("ISO: {} install files; {} SMK videos; {} legacy 3dfx drivers skipped",
        inventory.files.len(),inventory.video_files,inventory.skipped_legacy_drivers));
    let parent=destination.parent().ok_or("Invalid installation parent")?;
    // Disk preflight and validation are complete before creating any working
    // directory. Temp contains only the required source trees, not the CD's
    // obsolete installers/DirectX/Mystique/Verite directories.
    let temp=parent.join(format!(".scorchedplanetfix-extract-{}",std::process::id()));
    let staging=parent.join(format!(".splanet-setup-{}",std::process::id()));
    if temp.exists() || staging.exists() || temp.is_symlink() || staging.is_symlink(){
        return Err("Dossier temporaire déjà présent : nettoyage manuel requis (aucun fichier écrasé)".into());
    }
    fs::create_dir(&temp).map_err(|e|format!("Extraction temporaire impossible : {e}"))?;
    let install_result=(||{
        fs::create_dir(&staging).map_err(|e|format!("Installation temporaire impossible : {e}"))?;
        let total: u64=inventory.files.values().map(|a|a.size).sum();
        let mut done=0u64;
        // Phase 1: extract exact original SPLANET and VOODOO trees under temp.
        // The two legacy directories are preserved as two distinct sources.
        for (directory,files) in [
            ("splanet",&inventory.splanet_files),
            ("voodoo",&inventory.voodoo_files),
        ] {
            let base=temp.join(directory);
            fs::create_dir(&base).map_err(|e|e.to_string())?;
            for (key,asset) in files {
                let output=base.join(&asset.relative);
                if let Some(dir)=output.parent(){fs::create_dir_all(dir).map_err(|e|e.to_string())?;}
                let mut file=OpenOptions::new().write(true).create_new(true).open(&output)
                    .map_err(|e|format!("Création de temp/{directory}/{key} : {e}"))?;
                let mut writer=HashWriter{output:&mut file,hasher:Sha256::new(),
                    transferred:0,report:|delta|{
                        done+=delta;
                        report(10+done.saturating_mul(48)/total,100,"Extraction du CD dans temp…");
                    }};
                cat_node(&mut iso,&asset.node,&mut writer)
                    .map_err(|e|format!("Extraction {directory}/{key} : {e}"))?;
                if writer.transferred!=asset.size {
                    return Err(format!("Données incomplètes : {directory}/{key}"));
                }
                let original_hash=format!("{:x}",writer.hasher.finalize());
                file.flush().map_err(|e|e.to_string())?;
                file.sync_all().map_err(|e|e.to_string())?;
                drop(file);
                if file_digest(&output)?!=original_hash {
                    return Err(format!("Vérification extraction impossible : {directory}/{key}"));
                }
                let mut permissions=fs::metadata(&output).map_err(|e|e.to_string())?.permissions();
                permissions.set_readonly(false);
                fs::set_permissions(&output,permissions).map_err(|e|e.to_string())?;
                log(&format!("EXTRACT: temp/{directory}/{key} bytes={} sha256={original_hash}",asset.size));
            }
        }
        // Phase 2: copy the whole temp/splanet file tree to staged splanet,
        // then overlay *only* two original 3dfx gameplay files from voodoo.
        // Both copy sets are verified against the extracted temp sources.
        let mut copied=0u64;
        for (directory,files) in [
            ("splanet",&inventory.splanet_files),
            ("voodoo",&inventory.voodoo_files),
        ] {
            for (key,asset) in files {
                if directory=="voodoo" && !matches!(key.as_str(),"spvoodoo.exe"|"rwdlvood.dll") {
                    return Err(format!("Fichier VOODOO imprévu (refus de copie) : {key}"));
                }
                let source=temp.join(directory).join(&asset.relative);
                let target=staging.join(&asset.relative);
                if let Some(dir)=target.parent(){fs::create_dir_all(dir).map_err(|e|e.to_string())?;}
                let mut input=File::open(&source).map_err(|e|format!("Lecture temp/{directory}/{key} : {e}"))?;
                let mut output=OpenOptions::new().write(true).create_new(true).open(&target)
                    .map_err(|e|format!("Copie vers splanet/{key} : {e}"))?;
                let transferred=std::io::copy(&mut input,&mut output).map_err(|e|e.to_string())?;
                output.flush().map_err(|e|e.to_string())?;
                output.sync_all().map_err(|e|e.to_string())?;
                drop(output);
                if transferred!=asset.size || file_digest(&source)?!=file_digest(&target)? {
                    return Err(format!("Vérification copie impossible : {key}"));
                }
                copied+=transferred;
                report(58+copied.saturating_mul(35)/total,100,
                    if directory=="splanet" {"Copie de temp/splanet…"}
                    else {"Copie des composants temp/voodoo…"});
                log(&format!("COPY: temp/{directory}/{key} -> splanet/{key}, sha256={}",
                    file_digest(&target)?));
            }
        }
        // Phase 3: patch first, exactly as requested, and ensure the original
        // is backed up BEFORE changing any executable instruction bytes.
        report(94,100,"Sauvegarde .bak et application du correctif du jeu…");
        let game_exe=staging.join("spvoodoo.exe");
        let original_hash=edition.exe_sha256;
        log("PATCH-AUTO: backing up original spvoodoo.exe.bak before patch");
        let patch_report=scorched_planet_fix::execute(Action::Apply,&game_exe,None,true)
            .map_err(|e|format!("Correctif automatique impossible : {e}"))?;
        let backup=staging.join("spvoodoo.exe.bak");
        if !backup.is_file() || file_digest(&backup)?!=original_hash {
            return Err("Sauvegarde spvoodoo.exe.bak absente ou non conforme".into());
        }
        log(&format!("PATCH-AUTO OK: {patch_report}"));
        // Phase 4: generate the INI *after* the executable is verified.
        report(97,100,"Génération de splanet.ini…");
        let ini=staging.join("splanet.ini");
        let generated_ini=!ini.exists();
        let default_language=if edition.label=="FR/DE" {2}else{1};
        update_ini(&ini,&destination.join("video"),default_language)?;
        log(&format!("INI: edition={} language={} sha256={}",
            edition.label,default_language,file_digest(&ini)?));
        // Publish only after every validation, copy, backup, patch and INI
        // step has succeeded. Never expose a half-installed destination.
        if destination.exists() || destination.is_symlink() {
            return Err("splanet créé entre-temps : opération annulée".into());
        }
        fs::rename(&staging,destination)
            .map_err(|e|format!("Finalisation de l'installation : {e}"))?;
        report(100,100,"Installation terminée");
        let file_count=inventory.files.len()+usize::from(generated_ini)+1;
        log(&format!("INSTALL-OK: edition={} {} files {} source bytes, original .bak verified",
            edition.label,file_count,total));
        Ok(InstallReport{
            files:file_count,bytes:total,folder:destination.to_owned(),
            edition:edition.label,patch_report,
        })
    })();
    if install_result.is_err() {
        let _=fs::remove_dir_all(&staging);
        log("INSTALL-ERROR: staging removed; source CD image preserved");
    }
    if let Err(e)=fs::remove_dir_all(&temp) {
        log(&format!("TEMP-CLEANUP: unable to remove {} : {e}",temp.display()));
    }
    install_result
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn disc_discovery_treats_cue_bin_as_one_image(){
        let dir=std::env::temp_dir().join(format!("scorched-cue-discovery-{}",std::process::id()));
        let _=fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        fs::write(dir.join("DISC.BIN"),b"bin").unwrap();
        assert!(find_image_and_install(&dir).unwrap_err().contains("CUE"));
        let cue=dir.join("DISC.CuE");
        fs::write(&cue,b"FILE \"DISC.BIN\" BINARY").unwrap();
        assert_eq!(find_image_and_install(&dir).unwrap(),Some(cue));
        fs::write(dir.join(".scorchedplanetfix-cue-99-0.iso"),b"temporary").unwrap();
        assert_eq!(find_image_and_install(&dir).unwrap(),Some(dir.join("DISC.CuE")));
        fs::write(dir.join("other.ISO"),b"iso").unwrap();
        assert!(find_image_and_install(&dir).unwrap_err().contains("Plusieurs"));
        fs::remove_dir_all(dir).unwrap();
    }
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
        voodoo.add_child(TreeNode::new_file_with_location("GLIDE.DLL".into(),10,14336,10));
        voodoo.add_child(TreeNode::new_file_with_location("SST1INIT.DLL".into(),10,16384,10));
        voodoo.add_child(TreeNode::new_file_with_location("FXMEMMAP.VXD".into(),10,18432,10));
        root.add_child(sp);
        root.add_child(voodoo);
        let checked=inventory(&root).expect("CD sans splanet.ini doit être accepté");
        assert!(!checked.files.contains_key("splanet.ini"));
        assert_eq!(checked.video_files,1);
        assert_eq!(checked.skipped_legacy_drivers,3);
        assert_eq!(checked.splanet_files.len(),2); // geometry + video
        assert_eq!(checked.voodoo_files.len(),2); // exe + RenderWare DLL
        assert!(checked.splanet_files.contains_key("video/intro.smk"));
        assert_eq!(checked.voodoo_files["spvoodoo.exe"].relative,PathBuf::from("spvoodoo.exe"));
        assert_eq!(checked.splanet_files["video/intro.smk"].relative,
            PathBuf::from("video").join("intro.smk"));
        for legacy in ["glide.dll","fxmemmap.vxd","sst1init.dll"] {
            assert!(!checked.files.contains_key(legacy));
        }
    }
    #[test] fn install_tree_rejects_case_fold_collisions_and_file_dir_conflicts(){
        let mut root=TreeNode::new_directory("/".into());
        let mut sp=TreeNode::new_directory("sPlAnEt".into());
        sp.add_child(TreeNode::new_file_with_location("GEOMETRY.RWB".into(),10,2048,10));
        sp.add_child(TreeNode::new_file_with_location("geometry.rwb".into(),10,4096,10));
        let mut video=TreeNode::new_directory("VIDEO".into());
        video.add_child(TreeNode::new_file_with_location("INTRO.SMK".into(),10,8192,10));
        sp.add_child(video);
        let mut vd=TreeNode::new_directory("VoOdOo".into());
        vd.add_child(TreeNode::new_file_with_location("SPVOODOO.EXE".into(),10,10240,10));
        vd.add_child(TreeNode::new_file_with_location("RWDLVOOD.DLL".into(),10,12288,10));
        root.add_child(sp);root.add_child(vd);
        assert!(inventory(&root).err().expect("collision attendue").contains("casse différente"));
    }
    #[test] fn usr_edition_has_only_two_3dfx_components_and_no_legacy_drivers(){
        let mut root=TreeNode::new_directory("/".into());
        let mut sp=TreeNode::new_directory("SPLANET".into());
        sp.add_child(TreeNode::new_file_with_location("GEOMETRY.RWB".into(),10,2048,10));
        let mut video=TreeNode::new_directory("VIDEO".into());
        video.add_child(TreeNode::new_file_with_location("START.SMK".into(),10,4096,10));
        sp.add_child(video);
        let mut vd=TreeNode::new_directory("VOODOO".into());
        vd.add_child(TreeNode::new_file_with_location("SPVOODOO.EXE".into(),10,8192,10));
        vd.add_child(TreeNode::new_file_with_location("RWDLVOOD.DLL".into(),10,12288,10));
        root.add_child(sp);root.add_child(vd);
        let expected=inventory(&root).unwrap();
        assert_eq!(expected.skipped_legacy_drivers,0);
        assert_eq!(expected.voodoo_files.len(),2);
        assert_eq!(expected.splanet_files.len(),2);
        assert_eq!(expected.files.len(),4);
        assert!(expected.files.keys().all(|name|name==&name.to_ascii_lowercase()));
    }
    #[test] fn creates_missing_ini_with_local_video_and_defaults(){
        let dir=std::env::temp_dir().join(format!("scorched-new-ini-test-{}",std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let ini=dir.join("splanet.ini");
        assert!(!ini.exists());
        update_ini(&ini,Path::new(r"C:\splanet\video"),1).unwrap();
        let result=fs::read_to_string(&ini).unwrap();
        assert!(result.contains("[scorch]"));
        assert!(result.contains(r"video=C:\splanet\video"));
        assert!(result.contains("HighQualityVideo=1"));
        assert!(result.contains("language=1"));
        assert!(result.contains("settings=1 0 1 1 5 95 127 0 0 5 320"));
        assert!(result.contains("name=Player"));
        fs::remove_dir_all(&dir).unwrap();
    }
    #[test] fn edition_matching_is_exact_and_never_mix_and_matches(){
        for edition in EDITIONS {
            assert_eq!(identify_edition(edition.exe_sha256,edition.geometry_sha256),Some(edition));
        }
        assert!(identify_edition(EDITIONS[0].exe_sha256,EDITIONS[1].geometry_sha256).is_none());
        assert!(identify_edition("unknown",EDITIONS[0].geometry_sha256).is_none());
    }
    #[test] fn language_change_preserves_user_input_gamma_and_settings(){
        let dir=std::env::temp_dir().join(format!("scorched-ini-preserve-{}",std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let ini=dir.join("splanet.ini");
        fs::write(&ini,b"[scorch]\r\nvideo= c:\\old\\video\r\nlanguage= 4\r\nkeymap= 0 29 37\r\ngamma= 1.000000\r\njoystick= 0 0 0.000000\r\nHighQualityVideo= 0\r\nsettings= 1 0 1 1\r\nname= Testeur\r\n").unwrap();
        update_ini(&ini,Path::new(r"C:\new\video"),5).unwrap();
        let out=fs::read_to_string(&ini).unwrap();
        assert!(out.contains("language=5"));
        assert!(out.contains("HighQualityVideo=1"));
        assert!(out.contains(r"video=C:\new\video"));
        for v in ["keymap= 0 29 37","gamma= 1.000000","joystick= 0 0 0.000000","settings= 1 0 1 1","name= Testeur"]{
            assert!(out.contains(v),"missing {v}");
        }
        assert!(!out.contains("language= 4"));
        fs::remove_dir_all(&dir).unwrap();
    }
    #[test] fn ini_video_path_is_local(){
        let dir=std::env::temp_dir().join(format!("scorched-ini-test-{}",std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let ini=dir.join("splanet.ini");
        fs::write(&ini,b"[scorch]\r\nHighQualityVideo=1\r\nvideo=c:\\splanetcd\\splanet\\video\r\n").unwrap();
        update_ini(&ini,Path::new(r"C:\splanet\video"),4).unwrap();
        let result=fs::read_to_string(&ini).unwrap();
        assert!(result.contains(r"video=C:\splanet\video"));
        assert!(!result.contains("splanetcd"));
        fs::remove_dir_all(&dir).unwrap();
    }
}
