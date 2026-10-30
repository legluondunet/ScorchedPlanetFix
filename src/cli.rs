// SPDX-License-Identifier: GPL-3.0-or-later
//! ScorchedPlanetFix Windows --nogui mode: same edition-checked installer and patch.
use crate::{iso_install,logger::Logger,verify,nglide};
use scorched_planet_fix::{self, Action};
use std::ffi::OsString;
use std::fs;
use std::path::{Path,PathBuf};

const USAGE:&str="Utilisation :\n  ScorchedPlanetFix.exe --nogui\n  ScorchedPlanetFix.exe --nogui <image.iso>\n  ScorchedPlanetFix.exe --nogui <image.cue>\n  ScorchedPlanetFix.exe --nogui <image.cue> <piste.bin>\n\nSans chemin explicite, l'image est recherchée dans le répertoire courant.\nLe dossier splanet existant n'est jamais écrasé. Pour une installation\nneuve, spvoodoo.exe est sauvegardé sous spvoodoo.exe.bak avant modification.";

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
        let exe=scorched_planet_fix::find_installed_exe(&install)?;
        emit(logger,&mut sink,"Installation existante détectée. Vérification du correctif automatique…");
        let outcome=scorched_planet_fix::execute(Action::Apply,&exe,None,true)?;
        let edition=verify::profile_for_patched_exe(&exe)?;
        let language=verify::preferred_language(&install,edition.label);
        iso_install::update_ini(&install.join("splanet.ini"),language)?;
        let installed=nglide::copy_installed_glide(root,&install,|message|logger.write(message))?;
        if !installed {
            emit(logger,&mut sink,&format!("AVERTISSEMENT : nGlide manquant. Installation nécessaire depuis {}",nglide::OFFICIAL_URL));
        }
        emit(logger,&mut sink,&outcome);
        emit(logger,&mut sink,&format!("Configuration portable : video= video, language={language}"));
        emit(logger,&mut sink,&format!("Terminé. Original conservé : {}.bak",exe.display()));
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
    emit(logger,&mut sink,"Sauvegarde : splanet/spvoodoo.exe.bak");
    emit(logger,&mut sink,&format!("Correctif automatique : {}",report.patch_report));
    emit(logger,&mut sink,"Installation et correctif terminés. Le jeu n'a pas été lancé.");
    Ok(())
}

/// --verify: strict read-only verification of splanet beside the patcher.
pub fn verify_only(root:&Path,logger:&Logger,mut sink:impl FnMut(&str))->Result<(),String>{
    let report=verify::verify_install(&root.join("splanet"),|msg|logger.write(msg))?;
    emit(logger,&mut sink,&format!("Vérification OK : {} — {} fichiers vérifiés ({} octets), sauvegarde .bak conforme.",
        report.edition,report.checked_files,report.checked_bytes));
    Ok(())
}
/// --launch: verify the installation before spawning the game, then monitor
/// the game process exit code. Never launch an unverified executable.
pub fn launch_only(root:&Path,logger:&Logger,mut sink:impl FnMut(&str))->Result<(),String>{
    emit(logger,&mut sink,"Vérification intégrale de splanet avant lancement…");
    let mut child=verify::launch_verified(&root.join("splanet"),|msg|logger.write(msg))?;
    emit(logger,&mut sink,&format!("Jeu lancé, PID {} (vérification réussie).",child.id()));
    let status=child.wait().map_err(|e|format!("Surveillance du processus : {e}"))?;
    if !status.success(){
        return Err(format!("Le jeu a quitté avec le code {}",status.code()
            .map(|v|v.to_string()).unwrap_or_else(||"inconnu".into())));
    }
    emit(logger,&mut sink,"Le jeu s'est terminé normalement.");
    Ok(())
}

pub fn help(version:&str)->String{
    format!("ScorchedPlanetFix {version} — outil Windows x64 en ligne de commande
Usage :
  ScorchedPlanetFix.exe
  ScorchedPlanetFix.exe --nogui
  ScorchedPlanetFix.exe --nogui <image.iso>
  ScorchedPlanetFix.exe --nogui <image.cue> [piste.bin]
  ScorchedPlanetFix.exe --verify
  ScorchedPlanetFix.exe --launch
  ScorchedPlanetFix.exe --help
  ScorchedPlanetFix.exe --version

Sans argument / --nogui :
  Recherche un ISO ou CUE/BIN dans le répertoire courant, extrait le jeu
  dans splanet, sauvegarde spvoodoo.exe.bak et applique son correctif.
  Une installation existante est conservée ; l'exécutable peut être
  automatiquement corrigé, sans écraser les données du jeu.
--verify :
  Vérifie les fichiers du jeu, le patch et la sauvegarde dans splanet
  À CÔTÉ de ScorchedPlanetFix.exe. N'effectue aucune réparation.
--launch :
  Vérifie d'abord toute l'installation, puis lance le jeu ; attend sa sortie.
--help :
  Affiche les commandes, les chemins et cette version.
--version :
  Affiche uniquement la version.

nGlide (propriétaire, téléchargement et installation séparés) :
  {nglide_url}
  Une copie locale 32 bits glide.dll est nécessaire dans splanet.
  Lors du lancement, les variables nGlide sont limitées au processus
  du jeu : Vulkan si disponible, résolution bureau, aspect corrigé,
  60 Hz, VSync activée, gamma 1.0, logo 3dfx désactivé.

Toutes les erreurs sont signalées et inscrites dans ScorchedPlanetFix.log.
Aucun fichier original du CD ou du jeu n'est redistribué.",
    nglide_url=nglide::OFFICIAL_URL)
}
/// Exactly one standalone console entrypoint; no GUI is linked.
pub fn dispatch(arguments:&[OsString],version:&str)->Result<(),String>{
    let first=arguments.first().and_then(|v|v.to_str());
    if arguments.len()==1 && matches!(first,Some("--help")|Some("-h")) {
        println!("{}",help(version));
        return Ok(());
    }
    if arguments.len()==1 && first==Some("--version") {
        println!("ScorchedPlanetFix {version}");
        return Ok(());
    }
    let exe=std::env::current_exe().map_err(|e|format!("Exécutable introuvable : {e}"))?;
    let exe_root=exe.parent().ok_or("Dossier du patcher absent")?.to_path_buf();
    // Existing semantics: default and --nogui install in CWD. Verification
    // and launch always use the folder next to ScorchedPlanetFix.exe.
    let is_install=arguments.is_empty() || first==Some("--nogui");
    let root=if is_install {
        std::env::current_dir().map_err(|e|format!("Répertoire courant : {e}"))?
    }else{exe_root};
    let logger=Logger::new(&root,version)?;
    logger.system_information();
    let operation=match first{
        None=>run(&[],&root,&logger,|msg|println!("{msg}")),
        Some("--nogui")=>run(&arguments[1..],&root,&logger,|msg|println!("{msg}")),
        Some("--verify") if arguments.len()==1=>verify_only(&root,&logger,|msg|println!("{msg}")),
        Some("--launch") if arguments.len()==1=>launch_only(&root,&logger,|msg|println!("{msg}")),
        _=>Err(format!("Arguments inconnus. Utilisez --help.\n{}",help(version))),
    };
    if let Err(ref message)=operation{logger.error(message);}
    operation
}

#[cfg(test)]
mod tests{
    use super::*;
    fn args(s:&[&str])->Vec<OsString>{s.iter().map(OsString::from).collect()}
    #[test] fn help_lists_all_cli_options_and_version_without_gui(){
        let h=help("1.0.0-test1234");
        for literal in ["1.0.0-test1234","--nogui","--verify","--launch",
                        "--help","--version","image.iso","image.cue",
                        "glide.dll","60 Hz"]{
            assert!(h.contains(literal),"help missing {literal}");
        }
        assert!(!h.contains("Bouton Lancer"));
    }
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
