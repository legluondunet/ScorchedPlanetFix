// SPDX-License-Identifier: GPL-3.0-or-later
//! ScorchedPlanetFix Windows console installer, verifier and launcher.
use crate::{iso_install,logger::Logger,verify,nglide};
use scorched_planet_fix::{self, Action};
use std::ffi::OsString;
use std::fs;
use std::path::{Path,PathBuf};

const USAGE:&str="Usage:\n  ScorchedPlanetFix.exe\n  ScorchedPlanetFix.exe <image.iso>\n  ScorchedPlanetFix.exe <image.cue> [track.bin]\n\nWithout an explicit image path, the current working directory is searched.\nAn existing splanet directory is never overwritten. For a new install,\nspvoodoo.exe is backed up as spvoodoo.exe.bak before patching.";

#[derive(Debug,PartialEq,Eq)]
struct Arguments {
    image:Option<PathBuf>,
    bin:Option<PathBuf>,
}
fn parse_args(args:&[OsString])->Result<Arguments,String>{
    if args.len()>2 {return Err(format!("Too many arguments.\n{USAGE}"));}
    let image=args.first().map(PathBuf::from);
    let bin=args.get(1).map(PathBuf::from);
    if let Some(ref source)=image {
        let kind=source.extension().map(|e|e.to_string_lossy().to_ascii_lowercase());
        if !matches!(kind.as_deref(),Some("iso")|Some("cue")) {
            return Err(format!("Invalid disc image: expected an .iso or .cue file.\n{USAGE}"));
        }
        if bin.is_some() && kind.as_deref()!=Some("cue") {
            return Err(format!("An explicit .bin file requires a .cue file.\n{USAGE}"));
        }
    }
    if let Some(ref bin)=bin {
        if !bin.extension().is_some_and(|e|e.eq_ignore_ascii_case("bin")) {
            return Err("The second argument must be a .bin file".into());
        }
    }
    Ok(Arguments{image,bin})
}
fn regular_input(path:&Path)->Result<(),String>{
    let meta=fs::symlink_metadata(path).map_err(|e|format!("Disc image not found {}: {e}",path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink(){
        return Err(format!("Disc image must be a regular file; symlinks are not allowed: {}",path.display()));
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
        return Err("The splanet directory is a symlink; refusing".into());
    }
    if install.exists() {
        if args.image.is_some() {
            return Err("The splanet directory already exists: extraction refused; no files were overwritten".into());
        }
        let exe=scorched_planet_fix::find_installed_exe(&install)?;
        emit(logger,&mut sink,"Existing installation detected. Checking its patch...");
        let outcome=scorched_planet_fix::execute(Action::Apply,&exe,None,true)?;
        let edition=verify::profile_for_patched_exe(&exe)?;
        let language=verify::preferred_language(&install,edition.label);
        iso_install::update_ini(&install.join("splanet.ini"),language)?;
        for removed in iso_install::remove_unused_executables(&install,edition.label)? {
            emit(logger,&mut sink,&format!("Unused {removed} removed (original CD checksum verified)."));
        }
        let installed=nglide::copy_installed_glide(root,&install,|message|logger.write(message))?;
        if !installed {
            emit(logger,&mut sink,&format!("WARNING: nGlide is missing. Install it from {}",nglide::OFFICIAL_URL));
        }
        emit(logger,&mut sink,&outcome);
        emit(logger,&mut sink,&format!("Portable configuration: video= video, language={language}"));
        emit(logger,&mut sink,&format!("Done. Original backup preserved: {}.bak",exe.display()));
        return Ok(());
    }
    let source=match args.image {
        Some(p)=>p,
        None=>iso_install::find_image_and_install(root)?
            .ok_or("No ISO/CUE disc image found in the current working directory")?,
    };
    regular_input(&source)?;
    if let Some(ref b)=args.bin {regular_input(b)?;}
    emit(logger,&mut sink,&format!("Selected disc image: {}",source.display()));
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
    emit(logger,&mut sink,&format!("Detected edition: {}. {} files, {} game data bytes.",
        report.edition,report.files,report.bytes));
    emit(logger,&mut sink,"Backup: splanet/spvoodoo.exe.bak");
    emit(logger,&mut sink,&format!("Automatic patch: {}",report.patch_report));
    emit(logger,&mut sink,"Installation and patching completed. The game was not launched.");
    Ok(())
}

/// --verify: strict read-only verification of splanet beside the patcher.
pub fn verify_only(root:&Path,logger:&Logger,mut sink:impl FnMut(&str))->Result<(),String>{
    let report=verify::verify_install(&root.join("splanet"),|msg|logger.write(msg))?;
    emit(logger,&mut sink,&format!("Verification successful: {} — {} files checked ({} bytes); original .bak backup verified.",
        report.edition,report.checked_files,report.checked_bytes));
    Ok(())
}
/// --launch: verify the installation before spawning the game, then monitor
/// the game process exit code. Never launch an unverified executable.
pub fn launch_only(root:&Path,logger:&Logger,mut sink:impl FnMut(&str))->Result<(),String>{
    emit(logger,&mut sink,"Verifying the entire splanet installation before launching...");
    let mut child=verify::launch_verified(&root.join("splanet"),|msg|logger.write(msg))?;
    emit(logger,&mut sink,&format!("Game started, PID {} (verification successful).",child.id()));
    let status=child.wait().map_err(|e|format!("Failed while monitoring the game process: {e}"))?;
    if !status.success(){
        return Err(format!("The game exited with code {}",status.code()
            .map(|v|v.to_string()).unwrap_or_else(||"unknown".into())));
    }
    emit(logger,&mut sink,"The game exited normally.");
    Ok(())
}

pub fn help(version:&str)->String{
    format!("ScorchedPlanetFix {version} - Windows x64 command-line tool
Usage:
  ScorchedPlanetFix.exe
  ScorchedPlanetFix.exe <image.iso>
  ScorchedPlanetFix.exe <image.cue> [track.bin]
  ScorchedPlanetFix.exe --verify
  ScorchedPlanetFix.exe --launch
  ScorchedPlanetFix.exe --help
  ScorchedPlanetFix.exe --version

No arguments:
  Locate a single ISO or CUE/BIN in the current working directory, extract
  the game to splanet, back up spvoodoo.exe.bak, and apply its patch.
  Existing installations are preserved and can be patched automatically
  without overwriting game data.
<image.iso> / <image.cue> [track.bin]:
  Install from the specified disc image in the current working directory.
  Refuse to overwrite an existing splanet directory.
--verify:
  Verify game files, patch, and backup in the splanet directory NEXT
  TO ScorchedPlanetFix.exe. Does not modify any files.
--launch:
  Verify the entire installation, then start the game and wait for it to exit.
--help:
  Display commands, file paths, and this version.
--version:
  Display only the version.

nGlide (proprietary; download and install separately):
  {nglide_url}
  A locally installed 32-bit glide.dll is required in splanet.
  Game-specific nGlide settings are applied only to the game process:
  Vulkan when available, desktop resolution, aspect-ratio correction,
  60 Hz, VSync enabled, gamma 1.0, 3dfx splash screen disabled.

All errors are reported and written to ScorchedPlanetFix.log.
No copyrighted game files are redistributed.",
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
    let exe=std::env::current_exe().map_err(|e|format!("Cannot locate the executable: {e}"))?;
    let exe_root=exe.parent().ok_or("Cannot determine the patcher directory")?.to_path_buf();
    // Default and positional ISO/CUE installation operate in CWD.
    // Verification and launch always use the folder next to the executable.
    // Unknown switches, including removed legacy GUI switches, are rejected.
    let is_install=arguments.is_empty() || arguments.first()
        .is_some_and(|arg|!arg.to_string_lossy().starts_with('-'));
    let root=if is_install {
        std::env::current_dir().map_err(|e|format!("Current working directory: {e}"))?
    }else{exe_root};
    let logger=Logger::new(&root,version)?;
    logger.system_information();
    let operation=if is_install {
        run(arguments,&root,&logger,|msg|println!("{msg}"))
    }else{
        match first{
            Some("--verify") if arguments.len()==1=>verify_only(&root,&logger,|msg|println!("{msg}")),
            Some("--launch") if arguments.len()==1=>launch_only(&root,&logger,|msg|println!("{msg}")),
            _=>Err(format!("Unknown arguments. Use --help.\n{}",help(version))),
        }
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
        for literal in ["1.0.0-test1234","--verify","--launch",
                        "--help","--version","image.iso","image.cue",
                        "glide.dll","60 Hz"]{
            assert!(h.contains(literal),"help missing {literal}");
        }
        assert!(!h.contains("--nogui"),"obsolete GUI option must not appear in help");
        assert!(!h.contains("Launch button"));
    }
    #[test] fn installer_uses_no_arguments_or_positional_media(){
        assert!(args(&[]).is_empty());
        assert!(!args(&["game.ISO"])[0].to_string_lossy().starts_with('-'));
        assert!(!args(&["game.CUE","game.BIN"])[0].to_string_lossy().starts_with('-'));
        assert!(args(&["--nogui"])[0].to_string_lossy().starts_with('-'));
        assert!(help("test").contains("ScorchedPlanetFix.exe <image.iso>"));
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
        assert!(run(&args(&["disc.iso"]),&dir,&logger,|_|{}).unwrap_err().contains("already exists"));
        assert_eq!(fs::read(dir.join("splanet/SPVOODOO.EXE")).unwrap(),b"fixture");
        drop(logger);
        fs::remove_dir_all(&dir).unwrap();
    }
}
