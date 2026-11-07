use std::process::Command;

fn main() {
    println!("cargo:rerun-if-changed=.git/HEAD");
    // A verified open-source Glide build is embedded ONLY in the dedicated
    // GameFrame experiment. No closed-source nGlide binaries are distributed.
    println!("cargo:rustc-check-cfg=cfg(gameframe_embedded)");
    println!("cargo:rerun-if-env-changed=SCORCHED_GAMEFRAME_DLL_PATH");
    println!("cargo:rerun-if-env-changed=SCORCHED_GAMEFRAME_SHA256");
    if let Ok(path)=std::env::var("SCORCHED_GAMEFRAME_DLL_PATH"){
        let expected=std::env::var("SCORCHED_GAMEFRAME_SHA256")
            .expect("An embedded GameFrame x86 DLL requires its SHA-256 fingerprint");
        assert!(std::path::Path::new(&path).is_file(),"GameFrame DLL file not found");
        assert!(expected.len()==64 && expected.chars().all(|c| c.is_ascii_hexdigit()),
            "Invalid embedded GameFrame SHA-256");
        println!("cargo:rerun-if-changed={path}");
        println!("cargo:rustc-cfg=gameframe_embedded");
    }
    let sha = std::env::var("GITHUB_SHA").ok().filter(|s| !s.is_empty()).or_else(|| {
        let output=Command::new("git").args(["rev-parse", "--short=8", "HEAD"]).output().ok()?;
        output.status.success().then(|| String::from_utf8_lossy(&output.stdout).trim().to_string())
    }).unwrap_or_else(|| "unknown".into());
    let short: String = sha.chars().filter(|c| c.is_ascii_hexdigit()).take(8).collect();
    println!("cargo:rustc-env=SCORCHED_COMMIT={}",if short.is_empty(){"unknown"}else{&short});
}
