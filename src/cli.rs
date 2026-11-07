// SPDX-License-Identifier: GPL-3.0-or-later
//! Automatic, hash-verified Scorched Planet installer and launcher.
//! The only authority for game discovery is the executable's own folder.
use crate::{iso_install, logger::Logger, nglide, gameframe, verify};
use scorched_planet_fix::{self, Action};
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};

const USAGE: &str = "Usage:
  ScorchedPlanetFix.exe
  ScorchedPlanetFix.exe <image.iso>
  ScorchedPlanetFix.exe <image.cue> [track.bin]
  ScorchedPlanetFix.exe --3dfx-logo [image.iso | image.cue [track.bin]]
  ScorchedPlanetFix.exe --install-only [image.iso | image.cue [track.bin]]
  ScorchedPlanetFix.exe --verify
  ScorchedPlanetFix.exe --help
  ScorchedPlanetFix.exe --version

The executable finds splanet/ and CD images beside itself, not in the shell's
current directory. An existing installation is verified before every launch.
No unknown, invalid or damaged installation is replaced automatically.";

#[derive(Debug, PartialEq, Eq, Clone)]
struct Media {
    image: Option<PathBuf>,
    bin: Option<PathBuf>,
}

#[derive(Debug, PartialEq, Eq)]
enum Mode {
    Play,
    InstallOnly,
    VerifyOnly,
    Help,
    Version,
}

#[derive(Debug, PartialEq, Eq)]
struct CommandLine {
    mode: Mode,
    media: Media,
    splash: bool,
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
        if !track.extension().is_some_and(|e| e.eq_ignore_ascii_case("bin")) {
            return Err("The second disc image argument must be a .bin file".into());
        }
    }
    Ok(Media { image, bin })
}

fn parse_args(args: &[OsString]) -> Result<CommandLine, String> {
    let first = args.first().and_then(|a| a.to_str());
    let blank = Media { image: None, bin: None };
    match first {
        Some("--help") | Some("-h") if args.len() == 1 =>
            Ok(CommandLine { mode: Mode::Help, media: blank, splash: false }),
        Some("--version") if args.len() == 1 =>
            Ok(CommandLine { mode: Mode::Version, media: blank, splash: false }),
        Some("--verify") if args.len() == 1 =>
            Ok(CommandLine { mode: Mode::VerifyOnly, media: blank, splash: false }),
        Some("--install-only") =>
            Ok(CommandLine { mode: Mode::InstallOnly, media: parse_media(&args[1..])?, splash: false }),
        Some("--3dfx-logo") =>
            Ok(CommandLine { mode: Mode::Play, media: parse_media(&args[1..])?, splash: true }),
        Some(option) if option.starts_with('-') =>
            Err(format!("Unknown or incompatible option: {option}.\n{USAGE}")),
        _ =>
            Ok(CommandLine { mode: Mode::Play, media: parse_media(args)?, splash: false }),
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

/// One log beside ScorchedPlanetFix.exe for diagnostics, installation and play.
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
        emit(logger, &mut sink, "Existing installation detected. Performing explicitly requested safe migration.");
        let outcome = scorched_planet_fix::execute(Action::Apply, &exe, None, true)?;
        let edition = verify::profile_for_patched_exe(&exe)?;
        let language = verify::preferred_language(&install, edition.label);
        iso_install::update_ini(&install.join("splanet.ini"), language)?;
        for removed in iso_install::remove_unused_executables(&install, edition.label)? {
            emit(logger, &mut sink, &format!("Unused {removed} removed (original CD checksum verified)."));
        }
        let installed = gameframe::ensure_glide(root, &install, |message| logger.write(message))?;
        if !installed {
            emit(logger, &mut sink,
                &format!("WARNING: nGlide is missing. Install it from {}", nglide::OFFICIAL_URL));
        }
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
    let report = verify::verify_install(&root.join("splanet"), |msg| logger.write(msg))?;
    emit(logger, &mut sink, &format!(
        "Verification successful: {} — {} files checked ({} bytes); original .bak backup verified.",
        report.edition, report.checked_files, report.checked_bytes));
    Ok(())
}

/// Performs exactly ONE full read-only integrity audit, and starts the game
/// only after success. A nonzero process exit is logged without claiming that
/// it necessarily represents a crash.
fn verify_and_play(root: &Path, logger: &Logger, logo: bool,
                   mut sink: impl FnMut(&str)) -> Result<(), String> {
    emit(logger, &mut sink, "Verifying all immutable game files before launch...");
    let game=root.join("splanet");
    // Verify the complete game first, with no changes to any original data.
    // Provisioning the optional embedded GameFrame DLL is permitted only
    // once this edition's executable, original backup, INI and game-data
    // fingerprints are all confirmed.
    let report=verify::verify_game_assets(&game,|message|logger.write(message))?;
    if !gameframe::ensure_glide(root,&game,|message|logger.write(message))? {
        return Err(format!(
            "No compatible Glide DLL available. Install nGlide from {} or use a GameFrame-enabled build.",
            nglide::OFFICIAL_URL));
    }
    let mut child=verify::launch_preverified(report,&game,logo,
        |message|logger.write(message))?;
    emit(logger, &mut sink, &format!(
        "Game started, PID {} (full verification successful).", child.id()));
    let status = child.wait()
        .map_err(|e| format!("Failed while monitoring the game process: {e}"))?;
    if !status.success() {
        // Some historical games return 1 even when the player quits normally.
        // Do not silently turn every nonzero status into success.
        return Err(format!("The game exited with code {} (nonzero status; not proof of a crash)",
            status.code().map(|v| v.to_string()).unwrap_or_else(|| "unknown".into())));
    }
    emit(logger, &mut sink, "The game exited normally.");
    Ok(())
}

/// Auto-start decision uses only the folder containing the launcher.
/// An existing but invalid game folder blocks media fallback, replacement,
/// and launch; repair requires an explicit --install-only invocation.
fn session(root: &Path, cmd: &CommandLine, logger: &Logger,
           mut sink: impl FnMut(&str)) -> Result<(), String> {
    let game = root.join("splanet");
    if game.is_symlink() {
        return Err("The splanet directory is a symlink; refusing to inspect or replace it".into());
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
        let images = media_arguments(&cmd.media, root);
        run(&images, root, logger, |line| sink(line))?;
        if cmd.mode == Mode::InstallOnly {
            emit(logger, &mut sink, "Install-only requested: the game was not launched.");
            return Ok(());
        }
    }
    if cmd.mode == Mode::InstallOnly { return Ok(()); }
    verify_and_play(root, logger, cmd.splash, sink)
}

pub fn help(version: &str) -> String {
    format!("ScorchedPlanetFix {version} - Windows x64 automatic installer and launcher
{USAGE}

No arguments:
  If splanet exists beside this EXE: verify the complete installation and play.
  Otherwise, find one supported ISO or CUE/BIN beside this EXE, install,
  verify the complete installation and play.
  Damaged or unknown existing installs are never overwritten or launched.
<image.iso> / <image.cue> [track.bin]:
  Explicit original disc image; installs and plays only if splanet is absent.
--3dfx-logo:
  The same automatic flow, with the optional nGlide splash enabled.
  Requires your own 32-bit splanet/3DfxSpl.dll; may precede an image path.
--install-only:
  Install from an image without playing, or explicitly migrate a recognized
  older installation. It never overwrites unknown or modified executables.
  After installing nGlide, --install-only can also copy your local Glide DLL.
--verify:
  Read-only SHA-256 audit of splanet beside this EXE; never plays or modifies.
--help / --version:
  Show usage or executable version without running the game.

nGlide is proprietary freeware and not bundled:
  {nglide_url}
  A local 32-bit glide.dll is required in splanet for --verify and play.
  Portable nGlide options are applied to the game process only.

All diagnostics, including hardware inventory, go to ScorchedPlanetFix.log
beside this EXE. No proprietary game files are redistributed.",
        nglide_url = nglide::OFFICIAL_URL)
}

pub fn dispatch(arguments: &[OsString], version: &str) -> Result<(), String> {
    let cmd = parse_args(arguments)?;
    match cmd.mode {
        Mode::Help => { println!("{}", help(version)); return Ok(()); }
        Mode::Version => { println!("ScorchedPlanetFix {version}"); return Ok(()); }
        _ => {}
    }
    let exe = std::env::current_exe()
        .map_err(|e| format!("Cannot locate the executable: {e}"))?;
    let root = exe.parent()
        .ok_or("Cannot determine the patcher directory")?.to_path_buf();
    let logger = Logger::new(&root, version)?;
    logger.system_information();
    let result = if cmd.mode == Mode::VerifyOnly {
        verify_only(&root, &logger, |msg| println!("{msg}"))
    } else {
        session(&root, &cmd, &logger, |msg| println!("{msg}"))
    };
    if let Err(ref message) = result { logger.error(message); }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    fn args(items: &[&str]) -> Vec<OsString> {
        items.iter().map(OsString::from).collect()
    }
    #[test]
    fn command_line_modes_and_logo() {
        for cmd in ["--help", "-h", "--version", "--verify"] {
            assert!(parse_args(&args(&[cmd])).is_ok(), "{cmd}");
        }
        assert_eq!(parse_args(&args(&[])).unwrap().mode, Mode::Play);
        let logo = parse_args(&args(&["--3dfx-logo"])).unwrap();
        assert!(logo.splash && logo.mode == Mode::Play);
        assert!(parse_args(&args(&["--3dfx-logo", "disc.iso"])).unwrap().splash);
        assert_eq!(parse_args(&args(&["--install-only"])).unwrap().mode, Mode::InstallOnly);
        assert_eq!(parse_args(&args(&["--install-only", "DISC.CuE", "DISC.BIN"])).unwrap().media.bin,
            Some(PathBuf::from("DISC.BIN")));
        for invalid in [
            vec!["--launch"], vec!["--launch", "--3dfx-logo"],
            vec!["--verify", "disc.iso"], vec!["--install-only", "--3dfx-logo"],
            vec!["--3dfx-logo", "--install-only"], vec!["--unknown"],
            vec!["--help", "anything"], vec!["--version", "extra"],
            vec!["disc.iso", "disc.bin"], vec!["disc.cue", "bad.iso"],
            vec!["disc.iso", "anything", "extra"]
        ] {
            assert!(parse_args(&args(&invalid)).is_err(), "{invalid:?}");
        }
    }
    #[test]
    fn usage_has_no_legacy_launch_option() {
        let h = help("1.1.0-deadbeef");
        for word in ["1.1.0-deadbeef", "--install-only", "--verify",
                     "--3dfx-logo", "image.iso", "image.cue",
                     "glide.dll", "ScorchedPlanetFix.log"] {
            assert!(h.contains(word), "missing {word}");
        }
        assert!(!h.contains("ScorchedPlanetFix.exe --launch"));
        assert!(!h.contains("--nogui"));
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
        let explicit = parse_args(&args(&["DISC.ISO"])).unwrap();
        assert!(session(&root, &explicit, &logger, |_| {}).unwrap_err().contains("refusing to overwrite"));
        assert_eq!(fs::read(root.join("splanet/spvoodoo.exe")).unwrap(), b"unknown game exe");
        assert!(!root.join("splanet/spvoodoo.exe.bak").exists());
        assert_eq!(fs::read(root.join("DISC.ISO")).unwrap(), b"not-a-real-disc");
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
