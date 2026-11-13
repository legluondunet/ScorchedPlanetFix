// SPDX-License-Identifier: GPL-3.0-or-later
//! Automatic, hash-verified Scorched Planet installer and launcher.
//! The only authority for game discovery is the executable's own folder.
use crate::{iso_install, logger::Logger, gameframe, verify};
use scorched_planet_fix::{self, Action};
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};

const USAGE: &str = "Usage:
  ScorchedPlanetFix.exe
  ScorchedPlanetFix.exe <image.iso>
  ScorchedPlanetFix.exe <image.cue> [track.bin]
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
            Ok(CommandLine { mode: Mode::Help, media: blank }),
        Some("--version") if args.len() == 1 =>
            Ok(CommandLine { mode: Mode::Version, media: blank }),
        Some("--verify") if args.len() == 1 =>
            Ok(CommandLine { mode: Mode::VerifyOnly, media: blank }),
        Some("--install-only") =>
            Ok(CommandLine { mode: Mode::InstallOnly, media: parse_media(&args[1..])? }),
        Some(option) if option.starts_with('-') =>
            Err(format!("Unknown or incompatible option: {option}.\n{USAGE}")),
        _ =>
            Ok(CommandLine { mode: Mode::Play, media: parse_media(args)? }),
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
    let report = verify::verify_install(&root.join("splanet"), |msg| logger.write(msg))?;
    emit(logger, &mut sink, &format!(
        "Verification successful: {} — {} files checked ({} bytes); original .bak backup verified.",
        report.edition, report.checked_files, report.checked_bytes));
    Ok(())
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
<image.iso> / <image.cue> [track.bin]:
  Explicit original disc image; installs and plays only if splanet is absent.
--install-only:
  Install from an image without playing, or explicitly migrate a recognized
  older installation. It never overwrites unknown or modified executables.
  A missing or different regular Glide DLL is replaced from the verified
  embedded GameFrame after moving the old DLL to glide.dll.bak.
  An existing regular glide.dll.bak is replaced, never multiplied.
--verify:
  Read-only SHA-256 audit of splanet beside this EXE; never plays or modifies.
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
        for cmd in ["--help", "-h", "--version", "--verify"] {
            assert!(parse_args(&args(&[cmd])).is_ok(), "{cmd}");
        }
        assert_eq!(parse_args(&args(&[])).unwrap().mode, Mode::Play);
        assert_eq!(parse_args(&args(&["--install-only"])).unwrap().mode, Mode::InstallOnly);
        assert_eq!(parse_args(&args(&["--install-only", "DISC.CuE", "DISC.BIN"])).unwrap().media.bin,
            Some(PathBuf::from("DISC.BIN")));
        for invalid in [
            vec!["--launch"], vec!["--3dfx-logo"], vec!["--launch", "--3dfx-logo"],
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
        let h = help("2.0.1-deadbeef");
        for word in ["2.0.1-deadbeef", "--install-only", "--verify",
                     "GameFrame", "image.iso", "image.cue",
                     "glide.dll", "ScorchedPlanetFix_installer.log", "ScorchedPlanetFix.log"] {
            assert!(h.contains(word), "missing {word}");
        }
        assert!(!h.contains("ScorchedPlanetFix.exe --launch"));
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
