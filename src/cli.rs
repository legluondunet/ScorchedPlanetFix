// SPDX-License-Identifier: GPL-3.0-or-later
//! ScorchedPlanetFix Windows --nogui mode: same edition-checked installer and patch.
use crate::{iso_install,logger::Logger};
use scorched_planet_fix::{self, Action};
use std::ffi::OsString;
use std::fs;
use std::path::{Path,PathBuf};

const USAGE:&str="Utilisation :\n  ScorchedPlanetFix.exe --nogui\n  ScorchedPlanetFix.exe --nogui <image.iso>\n  ScorchedPlanetFix.exe --nogui <image.cue>\n  ScorchedPlanetFix.exe --nogui <image.cue> <piste.bin>\n\nSans chemin explicite, l'image est recherchée dans le répertoire courant.\nLe dossier splanet existant n'est jamais écrasé. SPVOODOO.EXE est sauvegardé\nsous SPVOODOO.EXE.bak avant toute modification.";

#[derive(Debug,PartialEq,Eq)]
struct Arguments {
    image:Option<PathBuf>,
    bin:Option<PathBuf>,
}
fn parse_args(args:&[OsString])->Result<Arguments,String>{
    if args.len()>2 {return Err(format!("Trop d'arguments.\n{USAGE}"));}
    let image=args.first().map(PathBuf::from);
    let bin=args.get(1).map(PathBuf::from);
    if let Some(ref source)=image {
        let kind=source.extension().map(|e|e.to_string_lossy().to_ascii_lowercase());
        if !matches!(kind.as_deref(),Some("iso")|Some("cue")) {
            return Err(format!("Image invalide : indiquer un fichier .iso ou .cue.\n{USAGE}"));
        }
        if bin.is_some() && kind.as_deref()!=Some("cue") {
            return Err(format!("Le .bin explicite nécessite un .cue.\n{USAGE}"));
        }
    }
    if let Some(ref bin)=bin {
        if !bin.extension().is_some_and(|e|e.eq_ignore_ascii_case("bin")) {
            return Err("La seconde position doit contenir un fichier .bin".into());
        }
    }
    Ok(Arguments{image,bin})
}
fn regular_input(path:&Path)->Result<(),String>{
    let meta=fs::symlink_metadata(path).map_err(|e|format!("Image introuvable {} : {e}",path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink(){
        return Err(format!("Image non régulière ou lien symbolique interdit : {}",path.display()));
    }
    Ok(())
}
/// Sends informational events to the caller's console and the file logger.
/// No GUI is ever created by this function.
fn emit(logger:&Logger,sink:&mut impl FnMut(&str),msg:&str){
    logger.write(&format!("CLI: {msg}"));
    sink(msg);
}
pub fn run(arguments:&[OsString],root:&Path,
    logger:&Logger,mut sink:impl FnMut(&str))->Result<(),String>{
    let args=parse_args(arguments)?;
     let install=root.join("splanet");
    if install.is_symlink() {
        return Err("Dossier splanet : lien symbolique refusé".into());
    }
    if install.exists() {
        if args.image.is_some() {
            return Err("Le dossier splanet existe déjà : extraction refusée, aucun fichier écrasé".into());
        }
        if !install.is_dir() || !install.join("SPVOODOO.EXE").is_file() {
            return Err("Dossier splanet incomplet : SPVOODOO.EXE absent".into());
        }
        emit(logger,&mut sink,logger,&mut sink,"Installation existante détectée. Vérification du correctif automatique…");
        let outcome=scorched_planet_fix::execute(Action::Apply,&install.join("SPVOODOO.EXE"),None,true)?;
        emit(logger,&mut sink,logger,&mut sink,&outcome);
        emit(logger,&mut sink,logger,&mut sink,"Terminé. SPVOODOO.EXE.bak contient l'exécutable original.");
        return Ok(());
    }
    let source=match args.image {
        Some(p)=>p,
        None=>iso_install::find_image_and_install(root)?
            .ok_or("Aucune image ISO/CUE trouvée dans le répertoire courant")?,
    };
    regular_input(&source)?;
    if let Some(ref b)=args.bin {regular_input(b)?;}
    emit(logger,&mut sink,&format!("Image sélectionnée : {}",source.display()));
    let report=iso_install::install_image(&source,args.bin.as_deref(),&install,
        |done,total,status|{
            // Progress output is intentionally throttled to avoid a line for
            // every file-sector; log retains all detailed extraction hashes.
            if done==0 || done==total || done%10==0 {
                let line=format!("{done}/{total}% {status}");
                sink(&line);
            }
        },
        |message|logger.write(message))?;
    emit(logger,&mut sink,&format!("Edition reconnue : {}. {} fichiers, {} octets de jeu.",
        report.edition,report.files,report.bytes));
    emit(logger,&mut sink,"Sauvegarde : splanet/SPVOODOO.EXE.bak");
    emit(logger,&mut sink,&format!("Correctif automatique : {}",report.patch_report));
    emit(logger,&mut sink,"Installation et correctif terminés. Le jeu n'a pas été lancé.");
    Ok(())
}

#[cfg(test)]
mod tests{
    use super::*;
    fn args(s:&[&str])->Vec<OsString>{s.iter().map(OsString::from).collect()}
    #[test] fn supported_command_line_forms(){
        assert_eq!(parse_args(&args(&[])).unwrap(),Arguments{image:None,bin:None});
        assert_eq!(parse_args(&args(&["game.ISO"])).unwrap().image,Some(PathBuf::from("game.ISO")));
        assert_eq!(parse_args(&args(&["game.CUE","game.BIN"])).unwrap(),
            Arguments{image:Some("game.CUE".into()),bin:Some("game.BIN".into())});
        assert!(parse_args(&args(&["game.ISO","game.BIN"])).is_err());
        assert!(parse_args(&args(&["game.bin"])).is_err());
        assert!(parse_args(&args(&["game.CUE","game.BIN","extra"])).is_err());
    }
    #[test] fn refuses_existing_install_when_explicit_image_requested(){
        let dir=std::env::temp_dir().join(format!("scorched-cli-test-{}",std::process::id()));
        let _=fs::remove_dir_all(&dir);
        fs::create_dir_all(dir.join("splanet")).unwrap();
        fs::write(dir.join("splanet/SPVOODOO.EXE"),b"fixture").unwrap();
        let logger=Logger::new(&dir,"test").unwrap();
        assert!(run(&args(&["disc.iso"]),&dir,&logger,|_|{}).unwrap_err().contains("existe"));
        assert_eq!(fs::read(dir.join("splanet/SPVOODOO.EXE")).unwrap(),b"fixture");
        drop(logger);
        fs::remove_dir_all(&dir).unwrap();
    }
}
