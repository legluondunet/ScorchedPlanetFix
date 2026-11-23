// SPDX-License-Identifier: GPL-3.0-or-later
//! CrimsonSkiesFix-style ownership layout: one game/, one root INI and logs,
//! state/ receipts, docs/ references; no third_party/ or dlls/ directories.
use std::fs::{self,OpenOptions};
use std::io::Write;
use std::path::Path;

pub const GAME: &str = "game";
pub const CONFIG: &str = "ScorchedPlanetFix.ini";
const FIRST_LAUNCH_MARKER: &str = "ScorchedPlanetFix.first-launch-logged.txt";

/// Shared default written to a new root INI and mirrored under docs/.
/// All user-facing settings have comments in both copies.
pub const INI_EXAMPLE: &str = "\
; ScorchedPlanetFix option descriptions\r\n\
; ScorchedPlanetFix options — Windows / Wine, retail 3dfx and Matrox OEM\r\n\
; Native game settings are stored separately in game/splanet.ini\r\n\
\r\n\
[Graphics]\r\n\
; Frame-rate limit for both renderers, 0=unlimited, 1..240=maximum FPS\r\n\
FPSLimit=60\r\n\
\r\n\
[Matrox]\r\n\
; Experimental ordered RGB565 dithering: 0=off (default), 1=on\r\n\
; Ignored when using the 3dfx/GameFrame edition\r\n\
Dither=0\r\n\
; Render index241 black ONLY for fingerprint-verified original Matrox icon01 atlas\r\n\
; 0=unchanged/reference (default), 1=experimental title-icon fidelity A/B\r\n\
Icon01Opaque=0\r\n\
\r\n\
[Cheats]\r\n\
; All cheats default to 0 (off). Automatic activation is NOT implemented yet.\r\n\
; 1 currently causes an explicit unsupported-feature error.\r\n\
; UnlimitedFuel: reserve for a future version-specific fuel consumption patch\r\n\
UnlimitedFuel=0\r\n\
; ALIAH: invulnerability; type the native code during gameplay for now\r\n\
; Some third-party code lists instead spell invulnerability ALLAH\r\n\
ALIAH=0\r\n\
; FATAL: grant all weapons and ammunition; type the native code in-game\r\n\
FATAL=0\r\n\
\r\n\
[Debug]\r\n\
; 0=disable runtime logs after first successful play (default)\r\n\
; 1=enable runtime logging for each game launch (noncumulative)\r\n\
; First game launch after installation is logged even when Log=0\r\n\
; ScorchedPlanetFix.log is created beside EXE for first run or Log=1\r\n\
; Installer diagnostics remain in ScorchedPlanetFix_installer.log\r\n\
Log=0\r\n";

fn regular_directory(path:&Path)->Result<(),String>{
    let metadata=fs::symlink_metadata(path).map_err(|e|format!("Cannot inspect {}: {e}",path.display()))?;
    if !metadata.is_dir() || metadata.file_type().is_symlink() {
        return Err(format!("Unsafe project directory: {}",path.display()));
    }
    Ok(())
}
fn ensure_dir(path:&Path)->Result<(),String>{
    match fs::symlink_metadata(path){
        Ok(_)=>regular_directory(path),
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>{
            match fs::create_dir(path){
                Ok(())=>regular_directory(path),
                Err(e)=>Err(format!("Cannot create {}: {e}",path.display())),
            }
        },
        Err(e)=>Err(format!("Cannot inspect {}: {e}",path.display())),
    }
}
fn write_new_if_missing(path:&Path,content:&str)->Result<(),String>{
    match fs::symlink_metadata(path){
        Ok(meta)=>{
            if !meta.is_file() || meta.file_type().is_symlink(){
                return Err(format!("Project document is not a regular file: {}",path.display()));
            }
            Ok(()) // Player documentation and previous release notes are retained.
        },
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>{
            let mut f=OpenOptions::new().create_new(true).write(true).open(path)
                .map_err(|e|format!("Cannot create {}: {e}",path.display()))?;
            f.write_all(content.as_bytes()).map_err(|e|e.to_string())?;
            f.sync_all().map_err(|e|e.to_string())
        },
        Err(e)=>Err(format!("Cannot inspect {}: {e}",path.display()))
    }
}
/// Create fix-owned supporting files only after an edition was recognized
/// and its installation verified. This is never called by read-only --verify,
/// --inspect, --help, --version or unchecked mode.
pub fn ensure_support_files(root:&Path,edition:&str)->Result<(),String>{
    regular_directory(&root.join(GAME))?;
    let docs=root.join("docs");
    let state=root.join("state");
    ensure_dir(&docs)?;
    ensure_dir(&state)?;
    write_new_if_missing(&docs.join("README.md"),include_str!("../README.md"))?;
    write_new_if_missing(&docs.join("LICENSE"),include_str!("../LICENSE"))?;
    write_new_if_missing(&docs.join("ScorchedPlanetFix.ini.example"),INI_EXAMPLE)?;
    let info=format!("ScorchedPlanetFix {}\nBuildCommit={}\nLayout=unified-1\n\
        GameFolder=game\nConfig=ScorchedPlanetFix.ini\n\
        Edition={edition}\nRenderer={}\n",
        env!("CARGO_PKG_VERSION"),env!("SCORCHED_COMMIT"),
        if edition=="matrox" {"MSI95"} else {"GameFrame"});
    write_new_if_missing(&docs.join("build-info.txt"),&info)?;
    let receipt=format!("ScorchedPlanetFix verified installation\n\
        Layout=unified-1\nGameFolder=game\nEdition={edition}\n\
        Note=This receipt is informational. Runtime always re-verifies game fingerprints.\n");
    write_new_if_missing(&state.join("ScorchedPlanetFix.install-receipt.txt"),&receipt)?;
    Ok(())
}
/// Called after verification and support-directory creation, immediately
/// before a NORMAL, verified game launch. --install-only must not consume it.
pub fn first_launch_pending(root:&Path)->Result<bool,String>{
    let path=root.join("state").join(FIRST_LAUNCH_MARKER);
    match fs::symlink_metadata(&path){
        Ok(meta) if meta.is_file()&&!meta.file_type().is_symlink()=>Ok(false),
        Ok(_)=>Err(format!("Unsafe first-launch receipt: {}",path.display())),
        Err(e) if e.kind()==std::io::ErrorKind::NotFound=>Ok(true),
        Err(e)=>Err(format!("Cannot inspect first-launch receipt: {e}")),
    }
}
/// Complete the first-PLAY recording only AFTER the game has actually run
/// and exited normally. Neither installation nor a failed process spawn
/// consumes this one-time diagnostics opportunity.
pub fn mark_first_launch_logged(root:&Path,session_id:&str)->Result<(),String>{
    if session_id.is_empty()||!session_id.bytes().all(|b|b.is_ascii_alphanumeric()||b==b'-'){
        return Err("Invalid first-launch session identifier".into());
    }
    regular_directory(&root.join("state"))?;
    let path=root.join("state").join(FIRST_LAUNCH_MARKER);
    if !first_launch_pending(root)?{return Ok(());}
    let mut file=OpenOptions::new().write(true).create_new(true).open(&path)
        .map_err(|e|format!("Cannot record first game launch {}: {e}",path.display()))?;
    let result=(||->Result<(),String>{
        file.write_all(format!("ScorchedPlanetFix first-launch runtime diagnostics\nSessionId={session_id}\n").as_bytes())
            .map_err(|e|e.to_string())?;
        file.sync_all().map_err(|e|e.to_string())
    })();
    if result.is_err(){let _=fs::remove_file(&path);}
    result
}
#[cfg(test)]
mod tests{
    use super::*;
    #[test]
    fn support_dirs_are_safe_and_existing_docs_are_preserved(){
        let root=std::env::temp_dir().join(format!("scorched-layout-{}",std::process::id()));
        let _=fs::remove_dir_all(&root);
        fs::create_dir_all(root.join(GAME)).unwrap();
        ensure_support_files(&root,"matrox").unwrap();
        assert!(root.join("docs/README.md").is_file());
        assert!(root.join("docs/LICENSE").is_file());
        assert!(root.join("docs/ScorchedPlanetFix.ini.example").is_file());
        assert!(root.join("state/ScorchedPlanetFix.install-receipt.txt").is_file());
        assert!(first_launch_pending(&root).unwrap());
        assert!(!root.join("third_party").exists());
        assert!(!root.join("dlls").exists());
        fs::write(root.join("docs/README.md"),b"user customization").unwrap();
        ensure_support_files(&root,"matrox").unwrap();
        assert_eq!(fs::read(root.join("docs/README.md")).unwrap(),b"user customization");
        assert!(first_launch_pending(&root).unwrap(), "install-only must not consume first-play logs");
        mark_first_launch_logged(&root,"test-session-17").unwrap();
        assert!(!first_launch_pending(&root).unwrap());
        mark_first_launch_logged(&root,"next-session-99").unwrap();
        let receipt=fs::read_to_string(root.join("state").join(FIRST_LAUNCH_MARKER)).unwrap();
        assert!(receipt.contains("SessionId=test-session-17"));
        assert!(!receipt.contains("next-session-99"));
        fs::remove_dir_all(root).unwrap();
    }
}
