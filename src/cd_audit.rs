//! Read-only report of an original Scorched Planet CD folder.
//! This does not run InstallShield, modify the disc, or distribute game data.

use std::fmt::Write as _;
use std::fs;
use std::path::Path;
use walkdir::WalkDir;

const MAX_INI_BYTES: u64 = 128 * 1024;

/// Build a shareable inventory without exposing the absolute path of the CD copy.
/// The report includes the contents of small, textual SETUP.INI files, but never
/// reads executable/archive payloads into memory.
pub fn audit_cd(root: &Path) -> Result<String, String> {
    if !root.is_dir() {
        return Err("Le dossier source du CD n'existe pas.".into());
    }

    let mut entries: Vec<(String, u64)> = Vec::new();
    let mut setup_ini: Vec<(String, String)> = Vec::new();
    let mut total_bytes = 0_u64;

    for entry in WalkDir::new(root).follow_links(false) {
        let entry = entry.map_err(|e| format!("Inventaire du CD impossible : {e}"))?;
        if entry.file_type().is_symlink() {
            return Err(format!(
                "Lien symbolique refuse pendant l'audit : {}",
                entry.path().display()
            ));
        }
        if !entry.file_type().is_file() {
            continue;
        }

        let relative = entry
            .path()
            .strip_prefix(root)
            .map_err(|e| e.to_string())?
            .to_string_lossy()
            .replace('\\', "/");
        let size = entry.metadata().map_err(|e| e.to_string())?.len();
        total_bytes = total_bytes
            .checked_add(size)
            .ok_or("Taille totale du CD trop grande.")?;

        if entry.file_name().to_string_lossy().eq_ignore_ascii_case("SETUP.INI") {
            let detail = if size > MAX_INI_BYTES {
                format!("Fichier trop volumineux ({size} octets), contenu non inclus.")
            } else {
                let raw = fs::read(entry.path())
                    .map_err(|e| format!("Lecture de {relative} impossible : {e}"))?;
                if raw.contains(&0) {
                    "Fichier binaire : contenu non inclus.".to_string()
                } else {
                    String::from_utf8_lossy(&raw).into_owned()
                }
            };
            setup_ini.push((relative.clone(), detail));
        }
        entries.push((relative, size));
    }

    entries.sort_by(|a, b| {
        a.0.to_ascii_lowercase()
            .cmp(&b.0.to_ascii_lowercase())
            .then(a.0.cmp(&b.0))
    });
    setup_ini.sort_by(|a, b| a.0.cmp(&b.0));

    let has_dir = |name: &str| {
        fs::read_dir(root).ok().into_iter().flatten().flatten().any(|entry| {
            entry.file_type().is_ok_and(|kind| kind.is_dir())
                && entry.file_name().to_string_lossy().eq_ignore_ascii_case(name)
        })
    };
    let contains = |name: &str| entries.iter().any(|(path, _)| path.eq_ignore_ascii_case(name));
    let has_archive = entries.iter().any(|(path, _)| {
        path.rsplit('/').next().is_some_and(|name| name.eq_ignore_ascii_case("COMPRESS.BIN"))
    });

    let mut output = String::new();
    writeln!(output, "Scorched Planet — inventaire du CD original").unwrap();
    writeln!(output, "Format du rapport : 1").unwrap();
    writeln!(output, "Fichiers : {}", entries.len()).unwrap();
    writeln!(output, "Taille : {total_bytes} octets").unwrap();
    writeln!(output, "SPLANET : {}", has_dir("SPLANET")).unwrap();
    writeln!(output, "VOODOO : {}", has_dir("VOODOO")).unwrap();
    writeln!(output, "SPVOODOO.EXE : {}", contains("VOODOO/SPVOODOO.EXE")).unwrap();
    writeln!(output, "RWDLVOOD.DLL : {}", contains("VOODOO/RWDLVOOD.DLL")).unwrap();
    writeln!(output, "COMPRESS.BIN : {has_archive}").unwrap();
    writeln!(output, "\nInventaire relatif (octets | fichier) :").unwrap();
    for (path, size) in &entries {
        writeln!(output, "{size} | {path}").unwrap();
    }

    if setup_ini.is_empty() {
        writeln!(output, "\nAucun fichier SETUP.INI accessible.").unwrap();
    } else {
        for (path, contents) in setup_ini {
            writeln!(output, "\n----- DEBUT {path} -----").unwrap();
            writeln!(output, "{contents}").unwrap();
            writeln!(output, "----- FIN {path} -----").unwrap();
        }
    }
    writeln!(
        output,
        "\nAttention : cet inventaire ne decompresse pas COMPRESS.BIN \
         et ne deduit pas les instructions d'InstallShield."
    )
    .unwrap();
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn report_lists_cd_files_and_setup_configuration() {
        let dir = tempfile::tempdir().unwrap();
        fs::create_dir_all(dir.path().join("SPLANET")).unwrap();
        fs::create_dir_all(dir.path().join("VOODOO")).unwrap();
        fs::create_dir_all(dir.path().join("INSTALL/WINDOWS")).unwrap();
        fs::write(dir.path().join("VOODOO/SPVOODOO.EXE"), b"exe").unwrap();
        fs::write(dir.path().join("SPLANET/COMPRESS.BIN"), b"archive").unwrap();
        fs::write(dir.path().join("INSTALL/WINDOWS/SETUP.INI"), b"[Setup]\r\nFoo=Bar\r\n").unwrap();

        let report = audit_cd(dir.path()).unwrap();
        assert!(report.contains("SPLANET : true"));
        assert!(report.contains("SPVOODOO.EXE : true"));
        assert!(report.contains("COMPRESS.BIN : true"));
        assert!(report.contains("INSTALL/WINDOWS/SETUP.INI"));
        assert!(report.contains("Foo=Bar"));
        assert!(!report.contains(dir.path().to_string_lossy().as_ref()));
    }

    #[test]
    fn report_rejects_missing_source() {
        assert!(audit_cd(Path::new("/missing-scorched-planet-cd")).is_err());
    }
}
