// SPDX-License-Identifier: GPL-3.0-or-later
//! Portable console interface. No Python, Wine, admin permissions or internet required.

use scorched_planet_fix::{execute, Action};
use std::env;
use std::ffi::OsString;
use std::path::PathBuf;
use std::process::ExitCode;

const VERSION: &str = env!("CARGO_PKG_VERSION");
const HELP: &str = r#"ScorchedPlanetFix — FadeLow patch for Scorched Planet (1996)

USAGE:
  scorched-planet-fix status  [--exe SPVOODOO.EXE]
  scorched-planet-fix copy    [--exe SPVOODOO.EXE] --output SPVOODOO-FadeLow.EXE
  scorched-planet-fix apply   [--exe SPVOODOO.EXE] --yes
  scorched-planet-fix restore [--exe SPVOODOO.EXE] --yes
  scorched-planet-fix --help

COMMANDS:
  status     Inspect SHA-256 and original-backup state; no changes.
  copy       Create a NEW patched executable; original is untouched.
  apply      Patch installed filename; keep verified original backup.
  restore    Restore installed EXE from its verified original backup.

OPTIONS:
  --exe FILE       Exact original retail 3dfx executable (default: SPVOODOO.EXE).
  --output FILE    Required for copy; output MUST NOT already exist.
  --yes            Required for apply/restore; close the game first.
  --version        Print version.

Only original SHA-256 fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a
is accepted for patching. No CD bypass, graphics-resolution experiment or
modified game executable is bundled. Original commercial game required.

Windows PowerShell:
  .\scorched-planet-fix.exe copy --exe "C:\Games\ScorchedPlanet\SPVOODOO.EXE" --output "C:\Games\ScorchedPlanet\SPVOODOO-FadeLow.EXE"

Linux / Lutris:
  ./scorched-planet-fix copy --exe /path/to/SPVOODOO.EXE --output /path/to/SPVOODOO-FadeLow.EXE
"#;

fn parse() -> Result<Option<(Action, PathBuf, Option<PathBuf>, bool)>, String> {
    let mut args = env::args_os();
    let _program = args.next();
    let first = match args.next() {
        None => { print!("{HELP}"); return Ok(None); }
        Some(v) => v,
    };
    let action = match first.to_str() {
        Some("--help" | "-h" | "help") => { print!("{HELP}"); return Ok(None); }
        Some("--version" | "-V") => {
            println!("ScorchedPlanetFix {VERSION}");
            return Ok(None);
        }
        Some("status") => Action::Status,
        Some("copy") => Action::Copy,
        Some("apply") => Action::Apply,
        Some("restore") => Action::Restore,
        _ => return Err(format!("Unknown action: {}. Use --help.", first.to_string_lossy())),
    };
    let mut exe = PathBuf::from("SPVOODOO.EXE");
    let mut output: Option<PathBuf> = None;
    let mut yes = false;
    let mut seen_exe = false;
    while let Some(arg) = args.next() {
        match arg.to_str() {
            Some("--exe" | "-e") => {
                if seen_exe { return Err("--exe specified twice".into()); }
                let v: OsString = args.next().ok_or("--exe requires a file path")?;
                exe = PathBuf::from(v);
                seen_exe = true;
            }
            Some("--output" | "-o") => {
                if output.is_some() { return Err("--output specified twice".into()); }
                let v: OsString = args.next().ok_or("--output requires a file path")?;
                output = Some(PathBuf::from(v));
            }
            Some("--yes") => {
                if yes { return Err("--yes specified twice".into()); }
                yes = true;
            }
            _ => return Err(format!("Unknown argument: {}. Use --help.", arg.to_string_lossy())),
        }
    }
    if action == Action::Copy && output.is_none() {
        return Err("copy requires --output FILE".into());
    }
    if action != Action::Copy && output.is_some() {
        return Err("--output is only valid with copy".into());
    }
    if yes && !matches!(action, Action::Apply | Action::Restore) {
        return Err("--yes is only valid with apply or restore".into());
    }
    Ok(Some((action, exe, output, yes)))
}

fn main() -> ExitCode {
    let result = parse().and_then(|parsed| match parsed {
        Some((action, exe, output, yes)) => execute(action, &exe, output.as_deref(), yes)
            .map(|message| { println!("{message}"); }),
        None => Ok(()),
    });
    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("ERROR: {err}");
            ExitCode::FAILURE
        }
    }
}
