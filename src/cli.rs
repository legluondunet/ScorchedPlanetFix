// SPDX-License-Identifier: GPL-3.0-or-later
//! Automatic, hash-verified Scorched Planet installer and launcher.
//! The only authority for game discovery is the executable's own folder.
use crate::{iso_install, logger::Logger, gameframe, verify, cue_image, matrox, matrox_runtime, matrox_iso};
use scorched_planet_fix::{self, Action};
use std::ffi::OsString;
use std::fs;
use std::collections::BTreeSet;
use std::fs::{File,OpenOptions};
use std::io::Write;
use isomage::{detect_and_parse_filesystem,TreeNode};
use std::path::{Path, PathBuf};
use std::process::{Child, Command};

const USAGE: &str = "Usage:
  ScorchedPlanetFix.exe
  ScorchedPlanetFix.exe --install-only
  ScorchedPlanetFix.exe --inspect image.iso
  ScorchedPlanetFix.exe --inspect image.cue [track.bin|track.img]
  ScorchedPlanetFix.exe --verbose
  ScorchedPlanetFix.exe --verify
  ScorchedPlanetFix.exe --launch-no-verify
  ScorchedPlanetFix.exe --matrox-verify
  ScorchedPlanetFix.exe --matrox-prepare
  ScorchedPlanetFix.exe --matrox-play
  ScorchedPlanetFix.exe --matrox-install image.iso|image.img|image.cue [track.bin|track.img]
  ScorchedPlanetFix.exe --matrox-install-only image.iso|image.img|image.cue [track.bin|track.img]
  ScorchedPlanetFix.exe --help
  ScorchedPlanetFix.exe --version

The executable finds splanet/ and CD images beside itself, not in the shell's
current directory. An existing installation is verified before every launch.
No unknown, invalid or damaged installation is replaced automatically.
--launch-no-verify is a direct, unverified launch; use only with trusted files.";

#[derive(Debug, PartialEq, Eq, Clone)]
struct Media {
    image: Option<PathBuf>,
    bin: Option<PathBuf>,
}

#[derive(Debug, PartialEq, Eq)]
enum Mode {
    Play,
    InstallOnly,
    Inspect,
    VerifyOnly,
    LaunchUnchecked,
    MatroxVerify,
    MatroxPrepare,
    MatroxPlay,
    MatroxInstall,
    MatroxInstallOnly,
    Help,
    Version,
}

#[derive(Debug, PartialEq, Eq)]
struct CommandLine {
    mode: Mode,
    media: Media,
}

fn parse_media(args: &[OsString]) -> Result<Media, String> {
    if args.len() > 2 {
        return Err(format!("Too many disc image arguments.\n{USAGE}"));
    }
    let image = args.first().map(PathBuf::from);
    let bin = args.get(1).map(PathBuf::from);
    if let Some(ref source) = image {
        let kind = source.extension().map(|e| e.to_string_lossy().to_ascii_lowercase());
        if !matches!(kind.as_deref(), Some("iso") | Some("cue")) {
            return Err(format!("Invalid disc image: expected an .iso or a .cue file.\n{USAGE}"));
        }
        if bin.is_some() && kind.as_deref() != Some("cue") {
            return Err(format!("A separate .bin file can only accompany a .cue file.\n{USAGE}"));
        }
    }
    if let Some(ref track) = bin {
        if !track.extension().is_some_and(|e| e.eq_ignore_ascii_case("bin") || e.eq_ignore_ascii_case("img")) {
            return Err("The second disc image argument must be a .bin or .img file".into());
        }
    }
    Ok(Media { image, bin })
}

fn parse_args(args:&[OsString])->Result<CommandLine,String>{
    let flags=args.iter().filter(|a|*a=="--verbose").count();
    if flags>1{return Err("--verbose specified twice".into());}
    let filtered=args.iter().filter(|a|*a!="--verbose").collect::<Vec<_>>();
    let name=filtered.first().and_then(|x|x.to_str());
    let blank=Media{image:None,bin:None};
    match name {
        None=>Ok(CommandLine{mode:Mode::Play,media:blank}),
        Some("--help"|"-h") if filtered.len()==1=>Ok(CommandLine{mode:Mode::Help,media:blank}),
        Some("--version") if filtered.len()==1=>Ok(CommandLine{mode:Mode::Version,media:blank}),
        Some("--verify") if filtered.len()==1=>Ok(CommandLine{mode:Mode::VerifyOnly,media:blank}),
        Some("--launch-no-verify") if filtered.len()==1=>Ok(CommandLine{mode:Mode::LaunchUnchecked,media:blank}),
        Some("--matrox-verify") if filtered.len()==1=>Ok(CommandLine{mode:Mode::MatroxVerify,media:blank}),
        Some("--matrox-prepare") if filtered.len()==1=>Ok(CommandLine{mode:Mode::MatroxPrepare,media:blank}),
        Some("--matrox-play") if filtered.len()==1=>Ok(CommandLine{mode:Mode::MatroxPlay,media:blank}),
        Some("--matrox-install" | "--matrox-install-only") if (2..=3).contains(&filtered.len())=>{
            let p = PathBuf::from(&filtered[1]);
            let is_cue = p.extension().is_some_and(|e| e.eq_ignore_ascii_case("cue"));
            if !is_cue && !p.extension().is_some_and(|e| e.eq_ignore_ascii_case("iso") || e.eq_ignore_ascii_case("img")) {
                return Err("Matrox install requires .iso, raw Mode1/2352 .img, or .cue with its BIN/IMG track".into());
            }
            let bin = filtered.get(2).map(PathBuf::from);
            if let Some(track) = &bin {
                if !is_cue || !track.extension().is_some_and(|e| e.eq_ignore_ascii_case("bin") || e.eq_ignore_ascii_case("img")) {
                    return Err("An explicit .bin/.img companion is only supported after a Matrox .cue file".into());
                }
            }
            Ok(CommandLine{
                mode:if name==Some("--matrox-install"){Mode::MatroxInstall}else{Mode::MatroxInstallOnly},
                media:Media{image:Some(p),bin},
            })
        },
        Some("--install-only") if filtered.len()==1=>Ok(CommandLine{mode:Mode::InstallOnly,media:blank}),
        Some("--inspect") if (2..=3).contains(&filtered.len())=>{
            let selected=filtered[1..].iter().map(|x|(*x).clone()).collect::<Vec<_>>();
            let media=parse_media(&selected)?;
            Ok(CommandLine{mode:Mode::Inspect,media})
        },
        Some(option) if option.starts_with('-')=>
            Err(format!("Unknown or incompatible option: {option}.\n{USAGE}")),
        Some(option)=>Err(format!(
            "Positional ISO/CUE media is no longer accepted: {option}; use --inspect.\n{USAGE}")),
    }
}
fn regular_input(path: &Path) -> Result<(), String> {
    let meta = fs::symlink_metadata(path)
        .map_err(|e| format!("Disc image not found {}: {e}", path.display()))?;
    if !meta.is_file() || meta.file_type().is_symlink() {
        return Err(format!("Disc image must be a regular file (no symlinks): {}", path.display()));
    }
    Ok(())
}

/// Resolve relative image paths against the patcher's directory, not the
/// launcher's shell working directory. Absolute input paths remain supported.
fn media_arguments(media: &Media, root: &Path) -> Vec<OsString> {
    [media.image.as_deref(), media.bin.as_deref()].into_iter().flatten()
        .map(|path| {
            if path.is_absolute() { path.to_path_buf() } else { root.join(path) }
                .into_os_string()
        })
        .collect()
}

/// Launcher-only log beside ScorchedPlanetFix.exe; the DLL has its own file.
fn emit(logger: &Logger, sink: &mut impl FnMut(&str), msg: &str) {
    logger.write(&format!("CLI: {msg}"));
    sink(msg);
}

/// Explicit install/repair operation. The normal startup does NOT modify
/// already-installed game files: an invalid existing install is refused.
/// This remains separate for --install-only and for a new CD installation.
pub fn run(arguments: &[OsString], root: &Path, logger: &Logger,
           mut sink: impl FnMut(&str)) -> Result<(), String> {
    let media = parse_media(arguments)?;
    let install = root.join("splanet");
    if install.is_symlink() {
        return Err("The splanet directory is a symlink; refusing".into());
    }
    if install.exists() {
        if media.image.is_some() {
            return Err("The splanet directory already exists: extraction refused; no files were overwritten".into());
        }
        let exe = scorched_planet_fix::find_installed_exe(&install)?;
        // --install-only must refuse an unsupported wrapper *before* any
        // patch, INI update or legacy executable cleanup touches game files.
        gameframe::preflight_glide(&install)?;
        emit(logger, &mut sink, "Existing installation detected. Performing explicitly requested safe migration.");
        let outcome = scorched_planet_fix::execute(Action::Apply, &exe, None, true)?;
        let edition = verify::profile_for_patched_exe(&exe)?;
        let language = verify::preferred_language(&install, edition.label);
        iso_install::update_ini(&install.join("splanet.ini"), language)?;
        for removed in iso_install::remove_unused_executables(&install, edition.label)? {
            emit(logger, &mut sink, &format!("Unused {removed} removed (original CD checksum verified)."));
        }
        gameframe::ensure_glide(root, &install, |message| logger.write(message))?;
        emit(logger, &mut sink, &outcome);
        emit(logger, &mut sink, &format!("Portable configuration: video= video, language={language}"));
        emit(logger, &mut sink, &format!("Migration complete. Original backup preserved: {}.bak", exe.display()));
        return Ok(());
    }

    let source = match media.image {
        Some(p) => p,
        None => iso_install::find_image_and_install(root)?
            .ok_or("No ISO/CUE disc image found beside ScorchedPlanetFix.exe")?,
    };
    regular_input(&source)?;
    if let Some(ref bin) = media.bin {
        regular_input(bin)?;
    }
    emit(logger, &mut sink, &format!("Selected disc image: {}", source.display()));
    let report = iso_install::install_image(&source, media.bin.as_deref(), &install,
        |done, total, status| {
            if done == 0 || done == total || done % 10 == 0 {
                sink(&format!("{done}/{total}% {status}"));
            }
        },
        |message| logger.write(message))?;
    emit(logger, &mut sink, &format!(
        "Detected edition: {}. {} files, {} game data bytes.",
        report.edition, report.files, report.bytes));
    emit(logger, &mut sink, "Backup: splanet/spvoodoo.exe.bak");
    emit(logger, &mut sink, &format!("Automatic patch: {}", report.patch_report));
    emit(logger, &mut sink, "Installation and patching completed.");
    Ok(())
}

/// Standalone --verify is always read-only and never launches a process.
pub fn verify_only(root: &Path, logger: &Logger,
                   mut sink: impl FnMut(&str)) -> Result<(), String> {
    // Preserve the 3dfx verification path when splanet/ already exists.
    // In this groundwork stage, Matrox can only be audited read-only;
    // installing and launching MSI95 will be integrated separately.
    if !root.join("splanet").exists() && root.join("scorchedplanet-matrox").exists() {
        return matrox_verify(root, logger, sink);
    }
    let report = verify::verify_install(&root.join("splanet"), |msg| logger.write(msg))?;
    emit(logger, &mut sink, &format!(
        "Verification successful: {} — {} files checked ({} bytes); original .bak backup verified.",
        report.edition, report.checked_files, report.checked_bytes));
    Ok(())
}

/// Always read-only, including when both Matrox and 3dfx installations exist.
fn matrox_verify(root: &Path, logger: &Logger,
                 mut sink: impl FnMut(&str)) -> Result<(), String> {
    let report = matrox::verify_original(&root.join("scorchedplanet-matrox"))?;
    emit(logger, &mut sink, &format!(
        "Matrox Mystique OEM original: {} files and {} bytes verified.",
        report.files, report.bytes));
    if root.join("scorchedplanet-matrox-runtime").exists() {
        matrox_runtime::verify_prepared(root)?;
        emit(logger, &mut sink, "Matrox runtime: isolated EXE and clean-room MSI95 DLL verified.");
    }
    Ok(())
}

/// Prepare the isolated Matrox runtime, then start the user-owned OEM game.
fn matrox_play(root: &Path, logger: &Logger,
               mut sink: impl FnMut(&str)) -> Result<(), String> {
    emit(logger, &mut sink, "Preparing or verifying isolated Matrox Mystique OEM runtime...");
    let runtime = matrox_runtime::prepare(root)?;
    emit(logger, &mut sink, &format!("Verified Matrox runtime: {}", runtime.display()));
    emit(logger, &mut sink, "Starting Matrox MSI95 backend (60 FPS max; no diagnostic time limit)...");
    let mut child = matrox_runtime::spawn(root)?;
    emit(logger, &mut sink, &format!("Matrox game started, PID {}.", child.id()));
    wait_for_game(&mut child, logger, sink)
}

/// Performs exactly ONE full read-only integrity audit, and starts the game
/// only after success. A nonzero process exit is logged without claiming that
/// it necessarily represents a crash.
fn verify_and_play(root: &Path, logger: &Logger,
                   mut sink: impl FnMut(&str)) -> Result<(), String> {
    emit(logger, &mut sink, "Verifying all immutable game files before launch...");
    let game=root.join("splanet");
    // Verify the complete game first, with no changes to any original data.
    // Provisioning the required embedded GameFrame DLL is permitted only
    // once this edition's executable, original backup, INI and game-data
    // fingerprints are all confirmed.
    let report=verify::verify_game_assets(&game,|message|logger.write(message))?;
    gameframe::ensure_glide(root,&game,|message|logger.write(message))?;
    let mut child=verify::launch_preverified(report,&game,logger.session_id(),
        |message|logger.write(message))?;
    emit(logger, &mut sink, &format!(
        "Game started, PID {} (full verification successful).", child.id()));
    wait_for_game(&mut child, logger, sink)
}

/// Explicit opt-out for debugging or custom game installations. No file
/// enumeration, existence checks, hashing, installation, patching, backup,
/// or Glide DLL verification happens here. The OS itself resolves the fixed
/// target path when spawning (and reports missing executables normally).
fn launch_unchecked(root: &Path, logger: &Logger,
                    mut sink: impl FnMut(&str)) -> Result<(), String> {
    let game = root.join("splanet");
    let executable = game.join("spvoodoo.exe");
    emit(logger, &mut sink,
        "WARNING: --launch-no-verify bypasses ALL game file existence/integrity checks and Glide DLL validation; no repair or installation will be attempted.");
    emit(logger, &mut sink,
        &format!("UNVERIFIED LAUNCH: executing {} with no file inspection.", executable.display()));
    let mut command = Command::new(&executable);
    command.current_dir(&game);
    gameframe::configure_process_unchecked(&mut command, &game, logger.session_id(),
        |message| logger.write(message))?;
    let mut child = command.spawn()
        .map_err(|e| format!("Unable to launch unchecked game {}: {e}", executable.display()))?;
    emit(logger, &mut sink,
        &format!("Game started, PID {} (UNVERIFIED; no file audit performed).", child.id()));
    wait_for_game(&mut child, logger, sink)
}

fn wait_for_game(child: &mut Child, logger: &Logger,
                 mut sink: impl FnMut(&str)) -> Result<(), String> {
    let status = child.wait()
        .map_err(|e| format!("Failed while monitoring the game process: {e}"))?;
    if !status.success() {
        // Real Windows/Wine/Proton sessions show that this 1996 game may
        // exit with code 1 after normal menu quit. Preserve the diagnostic,
        // but do not claim a launcher error without independent evidence.
        if status.code()==Some(1) {
            emit(logger, &mut sink,
                "Game exited with legacy code 1 (not necessarily a crash; check gameplay if unexpected).");
            return Ok(());
        }
        return Err(format!("The game exited with code {} (nonzero status; not proof of a crash)",
            status.code().map(|v| v.to_string()).unwrap_or_else(|| "unknown".into())));
    }
    emit(logger, &mut sink, "The game exited with code 0.");
    Ok(())
}

/// Auto-start decision uses only the folder containing the launcher.
/// An existing but invalid game folder blocks media fallback, replacement,
/// and launch; repair requires an explicit --install-only invocation.
/// Inspect an unsupported ISO or CUE without applying edition requirements,
/// installing a game, or writing to the normal runtime/installer log.
fn inspect_media(root:&Path,media:&Media,verbose:bool)->Result<(),String>{
    let image=media.image.as_ref().ok_or("Missing ISO/CUE source")?;
    let source=if image.is_absolute(){image.clone()}else{root.join(image)};
    regular_input(&source)?;
    let track=if let Some(raw)=media.bin.as_ref(){
        let selected=if raw.is_absolute(){raw.clone()}else{root.join(raw)};
        regular_input(&selected)?;
        Some(selected)
    }else{None};
    let destination=root.join("ScorchedPlanetFix_inspect.log");
    if fs::symlink_metadata(&destination).is_ok_and(
        |meta|!meta.file_type().is_file() || meta.file_type().is_symlink()) {
        return Err("Inspect report path must be a regular file".into());
    }
    let mut out=OpenOptions::new().create(true).write(true).truncate(true)
        .open(&destination).map_err(|e|format!("Cannot create inspect log: {e}"))?;
    writeln!(out,"ScorchedPlanetFix INSPECT v1\nSource={}\nTrack={}",
        source.display(),track.as_ref().map(|p|p.display().to_string()).unwrap_or_default())
        .map_err(|e|e.to_string())?;
    let result=(||->Result<(),String>{
        let temporary=if source.extension().is_some_and(|ext|ext.eq_ignore_ascii_case("cue")){
            Some(cue_image::normalize_for_inspection(&source,track.as_deref(),root,
                |_done,_total,_msg|{},|line|{
                    let safe=line.replace(['\r','\n']," ");
                    let _=writeln!(out,"CUE: {safe}");
                })?)
        }else {
            if track.is_some(){return Err("BIN/IMG data track requires a CUE file".into());}
            None
        };
        let image=temporary.as_ref().map(|t|t.path.as_path()).unwrap_or(&source);
        let mut input=File::open(image).map_err(|e|e.to_string())?;
        let len=input.metadata().map_err(|e|e.to_string())?.len();
        let tree=detect_and_parse_filesystem(&mut input,&image.to_string_lossy())
            .map_err(|e|format!("Unknown/unsupported CD filesystem: {e}"))?;
        writeln!(out,"DataBytes={len}\nPATH\tSIZE_BYTES\tOFFSET\tTYPE")
            .map_err(|e|e.to_string())?;
        fn walk(node:&TreeNode,path:&Path,depth:usize,entries:&mut usize,
                out:&mut File,verbose:bool)->Result<(),String>{
            if depth>40{return Err("ISO tree exceeds safe inspection depth".into());}
            for item in &node.children {
                *entries+=1;
                if *entries>25000{return Err("ISO file count exceeds safe inspection limit".into());}
                let name=item.name.replace(['\r','\n','\t'],"_");
                let full=path.join(name);
                let kind=if item.is_directory{"DIR"}else{"FILE"};
                writeln!(out,"{}\t{}\t{}\t{}",full.display(),item.size,
                    item.file_location.unwrap_or(0),kind).map_err(|e|e.to_string())?;
                if verbose {println!("INSPECT: {} bytes={} type={}",full.display(),item.size,kind);}
                if item.is_directory{walk(item,&full,depth+1,entries,out,verbose)?;}
            }
            Ok(())
        }
        let mut count=0usize;
        walk(&tree,Path::new(""),0,&mut count,&mut out,verbose)?;
        writeln!(out,"Entries={count}\nINSPECT_RESULT=COMPLETE")
            .map_err(|e|e.to_string())?;
        Ok(())
    })();
    if let Err(ref problem)=result {
        let _=writeln!(out,"INSPECT_RESULT=FAILED\nError={problem}");
    }
    out.flush().map_err(|e|e.to_string())?;
    println!("Inspection report: {}",destination.display());
    result
}

fn session(root: &Path, cmd: &CommandLine, logger: &Logger,
           mut sink: impl FnMut(&str)) -> Result<(), String> {
    if cmd.mode == Mode::LaunchUnchecked {
        // Deliberately bypass even the usual directory-presence and symlink
        // checks; leave all filesystem resolution to Command::spawn.
        return launch_unchecked(root,logger,sink);
    }
    // Matrox operations are explicitly selected, even if the retail 3dfx
    // installation also exists. Both backends remain independent.
    if cmd.mode == Mode::MatroxVerify {
        return matrox_verify(root, logger, sink);
    }
    if cmd.mode == Mode::MatroxPrepare {
        let runtime = matrox_runtime::prepare(root)?;
        emit(logger, &mut sink, &format!("Matrox runtime ready: {}", runtime.display()));
        return Ok(());
    }
    if cmd.mode == Mode::MatroxPlay {
        return matrox_play(root, logger, sink);
    }
    if cmd.mode == Mode::MatroxInstall || cmd.mode == Mode::MatroxInstallOnly {
        let chosen = cmd.media.image.as_ref().ok_or("Matrox CD path missing")?;
        let source = if chosen.is_absolute() { chosen.clone() } else { root.join(chosen) };
        let track = cmd.media.bin.as_ref().map(|raw| if raw.is_absolute() {
            raw.clone()
        } else {
            root.join(raw)
        });
        matrox_iso::install_media(root, &source, track.as_deref())?;
        emit(logger, &mut sink, &format!("Verified Matrox OEM CD installed from {}", source.display()));
        if cmd.mode == Mode::MatroxInstallOnly {
            return Ok(());
        }
        return matrox_play(root, logger, sink);
    }
    let game = root.join("splanet");
    if game.is_symlink() {
        return Err("The splanet directory is a symlink; refusing to inspect or replace it".into());
    }
    if !game.exists() && root.join("scorchedplanet-matrox").exists() {
        if cmd.mode == Mode::InstallOnly {
            let runtime = matrox_runtime::prepare(root)?;
            emit(logger, &mut sink, &format!("Matrox runtime prepared: {}", runtime.display()));
            return Ok(());
        }
        return matrox_play(root, logger, sink);
    }
    if game.exists() {
        if cmd.media.image.is_some() {
            return Err("splanet already exists; refusing to overwrite it with a CD image".into());
        }
        if cmd.mode == Mode::InstallOnly {
            emit(logger, &mut sink, "Install-only mode: checking and migrating recognized existing files.");
            return run(&[], root, logger, sink);
        }
        emit(logger, &mut sink,
            "Existing splanet detected. Game data will not be modified; checking all hashes before play.");
    } else {
        // Prefer a safe CUE descriptor for its referenced raw IMG. The IMG
        // itself must not be counted a second time as another Matrox disc.
        // Never guess when there really are multiple supported sources.
        let files = fs::read_dir(root).map_err(|e| e.to_string())?
            .map(|found| found.map(|item| item.path()).map_err(|e| e.to_string()))
            .collect::<Result<Vec<_>, _>>()?;
        let mut matrox_images = Vec::new();
        let mut cue_data_tracks = BTreeSet::new();
        for path in &files {
            if path.extension().is_some_and(|ext| ext.eq_ignore_ascii_case("cue"))
                && matrox_iso::recognized_media(root, path) {
                cue_data_tracks.insert(cue_image::data_track_source(path)?);
                matrox_images.push(path.clone());
            }
        }
        for path in &files {
            if path.extension().is_some_and(|ext| ext.eq_ignore_ascii_case("iso") || ext.eq_ignore_ascii_case("img"))
                && !cue_data_tracks.contains(path) && matrox_iso::recognized(path) {
                matrox_images.push(path.clone());
            }
        }
        if matrox_images.len() > 1 {
            return Err("Multiple Matrox OEM ISO/CUE/IMG images detected; use --matrox-install with an explicit path".into());
        }
        if let Some(image) = matrox_images.pop() {
            matrox_iso::install_media(root, &image, None)?;
            emit(logger, &mut sink, &format!("Matrox Mystique OEM automatically recognized and verified from {}", image.display()));
            if cmd.mode == Mode::InstallOnly {
                return Ok(());
            }
            return matrox_play(root, logger, sink);
        }
        let images = media_arguments(&cmd.media, root);
        run(&images, root, logger, |line| sink(line))?;
        if cmd.mode == Mode::InstallOnly {
            emit(logger, &mut sink, "Install-only requested: the game was not launched.");
            return Ok(());
        }
    }
    if cmd.mode == Mode::InstallOnly { return Ok(()); }
    verify_and_play(root, logger, sink)
}

pub fn help(version: &str) -> String {
    format!("ScorchedPlanetFix {version} - Windows x64 automatic installer and launcher
{USAGE}

No arguments:
  If splanet exists beside this EXE: verify the complete installation and play.
  Otherwise, find one supported ISO or CUE/BIN beside this EXE, install,
  verify the complete installation and play.
  Damaged or unknown existing installs are never overwritten or launched.
--inspect image.iso / image.cue [track.bin|track.img]:
  Inspect unsupported discs and write ScorchedPlanetFix_inspect.log.
--verbose:
  Extra diagnostics, combinable with commands.
--install-only:
  Install from an automatically discovered ISO/CUE without launching, or
  safely upgrade a recognized existing installation. User-modified EXEs
  and unknown files are never overwritten.
  A missing or different regular Glide DLL is moved to glide.dll.bak.
  An existing regular glide.dll.bak is replaced, never multiplied.
--verify:
  Read-only SHA-256 audit of splanet (3dfx) or an existing scorchedplanet-matrox
  OEM directory and its prepared runtime (if present); never plays or modifies.
--matrox-verify / --matrox-prepare / --matrox-play:
  Experimental OEM path, also selectable when 3dfx is installed. The first
  preparation requires the hash-verified clean-room
  ScorchedPlanetFix_MSI95_v035.dll beside the launcher (no original Matrox DLL).
  It creates scorchedplanet-matrox-runtime without modifying OEM original files.
  Matrox games run without the standalone probe's 300-second timeout.
  The clean-room MSI95 DLL is embedded alongside GameFrame (no extra DLL needed).
  Matrox ISO, raw IMG and CUE/IMG extraction is integrated.
--matrox-install image.iso|image.img|image.cue [track.bin|track.img]:
  Verify and extract a recognized OEM CD, prepare the isolated runtime, then play.
--matrox-install-only image.iso|image.img|image.cue [track.bin|track.img]:
  Verify and extract OEM original data without launching.
--launch-no-verify:
  Start splanet/spvoodoo.exe directly, without checking the presence,
  integrity, hashes, or identity of game files or the Glide DLL.
  Does not install, modify, migrate, or repair any files.
  Requires an existing playable installation; errors come from Windows/Wine.
  For trusted/debug installs only; untrusted game DLLs can be loaded.
--help / --version:
  Show usage or executable version without running the game.

Only open-source bundled GameFrame is supported in v2.
  At normal launch, after verifying immutable game files, an older or
  unknown regular splanet/glide.dll is moved to glide.dll.bak,
  replacing any previous regular backup rather than adding more files.
  The embedded GameFrame DLL is installed and verified before play.
  This backup replacement and both DLL hashes are logged.
  Non-regular files and symlinks are refused for safety.
  --verify is strictly read-only, including GameFrame validation.

Installer/launcher diagnostics and hardware inventory go to
ScorchedPlanetFix_installer.log beside this EXE. During gameplay, the
bundled GameFrame DLL writes ScorchedPlanetFix.log in the same directory.
Both logs share a session_id. No proprietary game files are redistributed.")
}

pub fn dispatch(arguments: &[OsString], version: &str) -> Result<(), String> {
    let cmd = parse_args(arguments)?;
    let verbose=arguments.iter().any(|a|a=="--verbose");
    match cmd.mode {
        Mode::Help => { println!("{}", help(version)); return Ok(()); }
        Mode::Version => { println!("ScorchedPlanetFix {version}"); return Ok(()); }
        _ => {}
    }
    let exe = std::env::current_exe()
        .map_err(|e| format!("Cannot locate the executable: {e}"))?;
    let root = exe.parent()
        .ok_or("Cannot determine the patcher directory")?.to_path_buf();
    if cmd.mode == Mode::Inspect {return inspect_media(&root,&cmd.media,verbose);}
    let logger = Logger::new(&root, version)?;
    logger.system_information();
    if verbose {
        let line=format!("VERBOSE: mode={:?} root={} game={}",
            cmd.mode,root.display(),root.join("splanet").display());
        println!("{line}");
        logger.write(&line);
    }
    let result = if cmd.mode == Mode::VerifyOnly {
        verify_only(&root, &logger, |msg| println!("{msg}"))
    } else {
        session(&root, &cmd, &logger, |msg| println!("{msg}"))
    };
    // main() writes the one final error, including early startup failures.
    // Avoid duplicate ERROR / ERROR (startup) entries in the same log.
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    fn args(items: &[&str]) -> Vec<OsString> {
        items.iter().map(OsString::from).collect()
    }
    #[test]
    fn command_line_modes_without_legacy_wrapper_flags() {
        for cmd in ["--help", "-h", "--version", "--verify", "--launch-no-verify"] {
            assert!(parse_args(&args(&[cmd])).is_ok(), "{cmd}");
        }
        assert_eq!(parse_args(&args(&[])).unwrap().mode, Mode::Play);
        assert_eq!(parse_args(&args(&["--install-only"])).unwrap().mode, Mode::InstallOnly);
        assert_eq!(parse_args(&args(&["--matrox-install", "oem.iso"])).unwrap().mode, Mode::MatroxInstall);
        assert_eq!(parse_args(&args(&["--matrox-install-only", "oem.img"])).unwrap().mode, Mode::MatroxInstallOnly);
        assert_eq!(parse_args(&args(&["--matrox-install-only", "Scorched_Planet.cue"])).unwrap().mode, Mode::MatroxInstallOnly);
        assert_eq!(parse_args(&args(&["--matrox-install", "Scorched_Planet.cue", "Scorched_Planet.img"])).unwrap().media.bin,
                   Some(PathBuf::from("Scorched_Planet.img")));
        for (cmd, mode) in [
            ("--matrox-play", Mode::MatroxPlay),
            ("--matrox-prepare", Mode::MatroxPrepare),
            ("--matrox-verify", Mode::MatroxVerify),
        ] {
            assert_eq!(parse_args(&args(&[cmd])).unwrap().mode, mode);
        }
        assert_eq!(parse_args(&args(&["--launch-no-verify"])).unwrap().mode, Mode::LaunchUnchecked);
        assert_eq!(parse_args(&args(&["--inspect","DISC.CuE","DISC.BIN"])).unwrap().media.bin,
            Some(PathBuf::from("DISC.BIN")));
        assert_eq!(parse_args(&args(&["--inspect","DISC.CuE","DISC.img"])).unwrap().mode,Mode::Inspect);
        assert_eq!(parse_args(&args(&["--verbose","--inspect","DISC.ISO"])).unwrap().mode,Mode::Inspect);
        assert_eq!(parse_args(&args(&["--install-only","--verbose"])).unwrap().mode,Mode::InstallOnly);
        for invalid in [
            vec!["--launch"], vec!["--launch-unchecked"], vec!["--3dfx-logo"], vec!["--launch", "--3dfx-logo"],
            vec!["--verify", "disc.iso"], vec!["--launch-no-verify", "disc.iso"],
            vec!["--install-only", "--launch-no-verify"],
            vec!["--launch-no-verify", "--verify"], vec!["--install-only", "--3dfx-logo"],
            vec!["--3dfx-logo", "--install-only"], vec!["--unknown"],
            vec!["--help", "anything"], vec!["--version", "extra"],
            vec!["disc.iso", "disc.bin"], vec!["disc.cue", "bad.iso"],
            vec!["disc.iso", "anything", "extra"],
            vec!["disc.iso"],vec!["disc.cue"],vec!["--install-only","disc.iso"],
            vec!["--inspect"],vec!["--inspect","disc.iso","track.bin"],
            vec!["--verbose","--verbose"],
            vec!["--matrox-install-only","disc.iso","track.img"],
            vec!["--matrox-install-only","disc.cue","track.iso"],
            vec!["--matrox-install","disc.cue","track.bin","fourth.img"]
        ] {
            assert!(parse_args(&args(&invalid)).is_err(), "{invalid:?}");
        }
    }
    #[test]
    fn usage_has_no_legacy_launch_option() {
        let h = help("2.0.2-deadbeef");
        for word in ["2.0.2-deadbeef", "--install-only", "--verify", "--launch-no-verify",
                     "--matrox-play", "--matrox-prepare", "--matrox-verify", "--matrox-install", "image.cue",
                     "GameFrame", "image.iso", "image.cue",
                     "glide.dll", "ScorchedPlanetFix_installer.log", "ScorchedPlanetFix.log"] {
            assert!(h.contains(word), "missing {word}");
        }
        // Only the obsolete exact --launch command is prohibited;
        // the explicitly named --launch-no-verify opt-out is supported.
        assert!(!h.lines().any(|line| line.trim() == "ScorchedPlanetFix.exe --launch"));
        assert!(!h.contains("--nogui"));
        assert!(!h.contains("--3dfx-logo"));
        assert!(!h.contains("nGlide is proprietary freeware and not bundled"));
        // Regression: CLI help must match the current automatic migration path.
        assert!(h.contains("moved to glide.dll.bak"));
        assert!(h.contains("glide.dll.bak"));
        assert!(h.contains("An existing regular glide.dll.bak is replaced"));
        assert!(!h.contains("unique .bak name"));
        assert!(!h.contains("Existing backups are never overwritten"));
        assert!(h.contains("--verify is strictly read-only"));
        assert!(!h.contains("move splanet/glide.dll aside"));
    }
    #[test]
    fn resolve_relative_images_against_exe_folder() {
        let root = Path::new(r"C:\Games\ScorchedPlanetFix");
        let m = Media { image: Some(PathBuf::from("disc.ISO")), bin: None };
        assert_eq!(media_arguments(&m, root), vec![root.join("disc.ISO").into_os_string()]);
    }
    #[test]
    fn existing_unverified_install_is_never_replaced_by_media() {
        let root = std::env::temp_dir().join(format!("scorched-autostart-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("splanet")).unwrap();
        fs::write(root.join("splanet/spvoodoo.exe"), b"unknown game exe").unwrap();
        fs::write(root.join("DISC.ISO"), b"not-a-real-disc").unwrap();
        let logger = Logger::new(&root, "test").unwrap();
        let no_args = parse_args(&[]).unwrap();
        let error = session(&root, &no_args, &logger, |_| {}).unwrap_err();
        assert!(error.contains("Unrecognized") || error.contains("Unknown") || error.contains("invalid"), "{error}");
        assert!(parse_args(&args(&["DISC.ISO"])).is_err());
        assert_eq!(fs::read(root.join("splanet/spvoodoo.exe")).unwrap(), b"unknown game exe");
        assert!(!root.join("splanet/spvoodoo.exe.bak").exists());
        assert_eq!(fs::read(root.join("DISC.ISO")).unwrap(), b"not-a-real-disc");
        drop(logger);
        fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn unchecked_launch_does_not_discover_media_create_game_dir_or_check_hashes() {
        let root=std::env::temp_dir().join(format!("scorched-unchecked-test-{}", std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        // A corrupt ISO must not be read or used for automatic installation.
        fs::write(root.join("DISC.ISO"),b"corrupted image data").unwrap();
        let logger=Logger::new(&root,"test").unwrap();
        let cmd=parse_args(&args(&["--launch-no-verify"])).unwrap();
        let mut events=Vec::new();
        let error=session(&root,&cmd,&logger,|line|events.push(line.to_owned())).unwrap_err();
        assert!(error.contains("Unable to launch unchecked game"),"{error}");
        assert!(events.iter().any(|line|line.contains("UNVERIFIED LAUNCH")));
        assert!(!events.iter().any(|line|line.contains("Verifying all immutable")));
        assert!(!root.join("splanet").exists());
        assert_eq!(fs::read(root.join("DISC.ISO")).unwrap(),b"corrupted image data");
        drop(logger);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn matrox_only_does_not_accidentally_enter_3dfx_install_path() {
        let root = std::env::temp_dir()
            .join(format!("scorched-matrox-guard-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("scorchedplanet-matrox")).unwrap();
        let logger = Logger::new(&root, "test").unwrap();
        let err = session(&root, &parse_args(&[]).unwrap(), &logger, |_| {}).unwrap_err();
        assert!(err.contains("Cannot inspect") || err.contains("size mismatch"), "{err}");
        assert!(!root.join("splanet").exists());
        let err = verify_only(&root, &logger, |_| {}).unwrap_err();
        assert!(err.contains("Cannot inspect") || err.contains("size mismatch"), "{err}");
        drop(logger);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn no_media_without_install_fails_without_creating_game_dir() {
        let root = std::env::temp_dir().join(format!("scorched-no-media-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let logger = Logger::new(&root, "test").unwrap();
        let error = session(&root, &parse_args(&[]).unwrap(), &logger, |_| {}).unwrap_err();
        assert!(error.contains("No ISO/CUE disc image"), "{error}");
        assert!(!root.join("splanet").exists());
        drop(logger);
        fs::remove_dir_all(root).unwrap();
    }
}
