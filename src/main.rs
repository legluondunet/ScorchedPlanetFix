// SPDX-License-Identifier: GPL-3.0-or-later
//! Pure Windows console ScorchedPlanetFix. No GUI, no GPL-incompatible bundled DLLs.
#[cfg(not(windows))]
compile_error!("ScorchedPlanetFix is a Windows x64 console program; run via Wine on Linux.");
#[cfg(windows)] mod diagnostics;
#[cfg(windows)] mod diagnostics_graphics;
#[cfg(windows)] mod cue_image;
#[cfg(windows)] mod cli;
#[cfg(windows)] mod iso_install;
#[cfg(windows)] mod logger;
#[cfg(windows)] mod nglide;
#[cfg(windows)] mod verify;
#[cfg(windows)] mod verify_manifest;

#[cfg(windows)]
fn main(){
    let version=format!("{}-{}",env!("CARGO_PKG_VERSION"),env!("SCORCHED_COMMIT"));
    let argv:Vec<std::ffi::OsString>=std::env::args_os().skip(1).collect();
    let result=cli::dispatch(&argv,&version);
    if let Err(message)=result {
        let directory=std::env::current_exe().ok().and_then(|p|p.parent().map(|x|x.to_path_buf()));
        logger::Logger::emergency_error(directory.as_deref(),&message);
        eprintln!("ScorchedPlanetFix : ERREUR : {message}");
        eprintln!("Détails : ScorchedPlanetFix.log");
        std::process::exit(1);
    }
}
