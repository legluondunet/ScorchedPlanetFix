//! Filesystem-only installer core. Does not run legacy setup executables.
//! Commercial game assets must be supplied by the end user.

use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

#[derive(Debug, Clone)]
pub struct InstallOptions {
    pub cd_root: PathBuf,
    pub destination: PathBuf,
    /// Scorched Planet's original language code: French=2, German=3.
    pub language: u8,
}

#[derive(Debug, Clone)]
pub struct FileToCopy {
    pub source: PathBuf,
    pub relative: PathBuf,
    pub size: u64,
}

#[derive(Debug, Clone)]
pub struct InstallPlan {
    pub files: Vec<FileToCopy>,
    pub total_bytes: u64,
    pub warnings: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct InstallResult {
    pub destination: PathBuf,
    pub files_copied: usize,
    pub total_bytes: u64,
    pub warnings: Vec<String>,
}

/// Finds an immediate child of a CD directory without requiring upper-case names.
fn find_directory(root: &Path, wanted: &str) -> Result<PathBuf, String> {
    let entries = fs::read_dir(root).map_err(|e| format!("{}: {e}", root.display()))?;
    for entry in entries {
        let entry = entry.map_err(|e| e.to_string())?;
        let ty = entry.file_type().map_err(|e| e.to_string())?;
        if ty.is_dir() && entry.file_name().to_string_lossy().eq_ignore_ascii_case(wanted) {
            return Ok(entry.path());
        }
    }
    Err(format!("Le dossier {wanted} manque dans {}", root.display()))
}

fn add_tree(
    tree: &Path,
    files: &mut BTreeMap<String, FileToCopy>,
) -> Result<(), String> {
    for entry in WalkDir::new(tree).follow_links(false) {
        let entry = entry.map_err(|e| format!("Erreur pendant l'inventaire: {e}"))?;
        let ty = entry.file_type();
        if ty.is_symlink() {
            return Err(format!(
                "Lien symbolique non autorise sur le CD : {}",
                entry.path().display()
            ));
        }
        if !ty.is_file() {
            continue;
        }
        let relative = entry
            .path()
            .strip_prefix(tree)
            .map_err(|e| e.to_string())?
            .to_path_buf();
        let key = relative.to_string_lossy().replace('\\', "/").to_ascii_lowercase();
        let size = entry.metadata().map_err(|e| e.to_string())?.len();
        // VOODOO takes priority over the shared SPLANET files for equal names.
        files.insert(key, FileToCopy {
            source: entry.path().to_path_buf(),
            relative,
            size,
        });
    }
    Ok(())
}

pub fn plan_install(options: &InstallOptions) -> Result<InstallPlan, String> {
    if !matches!(options.language, 2 | 3) {
        return Err("Langue non prise en charge (2=Francais, 3=Deutsch).".into());
    }
    if !options.cd_root.is_dir() {
        return Err("Choisir un dossier contenant les fichiers copies du CD.".into());
    }
    if options.destination.exists() {
        return Err(format!(
            "Le dossier de destination existe deja : {}",
            options.destination.display()
        ));
    }
    if options.destination.file_name().is_none() {
        return Err("Destination invalide.".into());
    }
    // Reject a destination inside the source tree, to prevent recursive copying.
    let source_abs = fs::canonicalize(&options.cd_root).map_err(|e| e.to_string())?;
    if let Ok(parent) = options
        .destination
        .parent()
        .ok_or("Destination sans dossier parent.")?
        .canonicalize()
    {
        if parent.starts_with(&source_abs) {
            return Err("La destination ne doit pas se trouver dans les fichiers du CD.".into());
        }
    }

    let shared = find_directory(&options.cd_root, "SPLANET")?;
    let voodoo = find_directory(&options.cd_root, "VOODOO")?;
    let mut mapping = BTreeMap::new();
    add_tree(&shared, &mut mapping)?;
    add_tree(&voodoo, &mut mapping)?;

    if !mapping.contains_key("spvoodoo.exe") {
        return Err("SPVOODOO.EXE n'a pas ete trouve dans les donnees de la version 3dfx.".into());
    }

    let mut warnings = Vec::new();
    if mapping.keys().any(|k| k.ends_with("compress.bin")) {
        warnings.push(
            "COMPRESS.BIN detecte : cette version copie l'archive, mais ne l'extrait pas. \
             Des ressources graphiques peuvent manquer avant une future prise en charge."
                .into(),
        );
    }
    if !mapping.contains_key("rwdlvood.dll") {
        warnings.push("RWDLVOOD.DLL est absent : verifier cette edition du CD.".into());
    }
    let files: Vec<_> = mapping.into_values().collect();
    let total_bytes = files.iter().map(|x| x.size).sum();
    Ok(InstallPlan {
        files,
        total_bytes,
        warnings,
    })
}

fn stage_path(destination: &Path) -> Result<PathBuf, String> {
    let parent = destination
        .parent()
        .ok_or("Dossier de destination invalide.")?;
    let name = destination
        .file_name()
        .ok_or("Nom de destination invalide.")?
        .to_string_lossy();
    Ok(parent.join(format!("{name}.scorched-planet-staging")))
}

/// Keep the existing INI data, only update the entries required by installation.
fn configure_ini(staging: &Path, destination: &Path, language: u8) -> Result<(), String> {
    let ini = staging.join("SPLANET.INI");
    let original = fs::read(&ini).unwrap_or_default();
    let text = String::from_utf8_lossy(&original);
    let mut lines: Vec<String> = Vec::new();
    let mut in_scorch = false;
    let mut found_section = false;
    let mut found_video = false;
    let mut found_language = false;
    let video_path = destination.join("VIDEO").to_string_lossy().to_string();

    for line in text.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with('[') && trimmed.ends_with(']') {
            // Finish the preceding [scorch] section before starting another.
            if in_scorch {
                if !found_video {
                    lines.push(format!("video={video_path}"));
                }
                if !found_language {
                    lines.push(format!("language={language}"));
                }
            }
            in_scorch = trimmed.eq_ignore_ascii_case("[scorch]");
            found_section |= in_scorch;
            lines.push(line.to_string());
        } else if in_scorch && trimmed.to_ascii_lowercase().starts_with("video=") {
            lines.push(format!("video={video_path}"));
            found_video = true;
        } else if in_scorch && trimmed.to_ascii_lowercase().starts_with("language=") {
            lines.push(format!("language={language}"));
            found_language = true;
        } else {
            lines.push(line.to_string());
        }
    }
    if !found_section {
        lines.push("[scorch]".into());
        in_scorch = true;
    }
    if in_scorch {
        if !found_video {
            lines.push(format!("video={video_path}"));
        }
        if !found_language {
            lines.push(format!("language={language}"));
        }
    }
    // Windows INI consumers expect CRLF.
    fs::write(ini, format!("{}\r\n", lines.join("\r\n")))
        .map_err(|e| format!("Impossible de configurer SPLANET.INI: {e}"))
}

/// Executes a preflighted plan. Emits (copied bytes, total bytes, displayed filename).
/// The final destination is never overwritten and becomes visible after all copies.
pub fn execute_install(
    options: &InstallOptions,
    plan: &InstallPlan,
    mut on_progress: impl FnMut(u64, u64, &str),
) -> Result<InstallResult, String> {
    if options.destination.exists() {
        return Err("La destination existe deja : aucune modification effectuee.".into());
    }
    let staging = stage_path(&options.destination)?;
    if staging.exists() {
        return Err(format!(
            "Un dossier temporaire existe deja. Verifiez-le sans l'effacer automatiquement : {}",
            staging.display()
        ));
    }
    let parent = options
        .destination
        .parent()
        .ok_or("Destination invalide.")?;
    fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    fs::create_dir(&staging).map_err(|e| e.to_string())?;

    let result: Result<(), String> = (|| {
        let mut copied_bytes = 0;
        for file in &plan.files {
            let output = staging.join(&file.relative);
            if let Some(dir) = output.parent() {
                fs::create_dir_all(dir).map_err(|e| format!("{}: {e}", dir.display()))?;
            }
            fs::copy(&file.source, &output)
                .map_err(|e| format!("Copie de {} impossible: {e}", file.source.display()))?;
            // Files copied from optical media often retain their read-only bit.
            // Installed saves and configuration must remain writable.
            if !file.relative.to_string_lossy().eq_ignore_ascii_case("GEOMETRY.RWB") {
                let mut permissions = fs::metadata(&output)
                    .map_err(|e| e.to_string())?
                    .permissions();
                permissions.set_readonly(false);
                fs::set_permissions(&output, permissions).map_err(|e| e.to_string())?;
            }
            copied_bytes += file.size;
            on_progress(copied_bytes, plan.total_bytes, &file.relative.to_string_lossy());
        }
        configure_ini(&staging, &options.destination, options.language)?;
        if options.destination.exists() {
            return Err("Destination creee entre-temps : installation interrompue.".into());
        }
        fs::rename(&staging, &options.destination)
            .map_err(|e| format!("Impossible de finaliser l'installation: {e}"))?;
        Ok(())
    })();
    if result.is_err() {
        // Only remove the unique staging directory created by this run.
        let _ = fs::remove_dir_all(&staging);
    }
    result?;

    Ok(InstallResult {
        destination: options.destination.clone(),
        files_copied: plan.files.len(),
        total_bytes: plan.total_bytes,
        warnings: plan.warnings.clone(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fixture() -> (tempfile::TempDir, InstallOptions) {
        let root = tempfile::tempdir().unwrap();
        let disc = root.path().join("CD");
        fs::create_dir_all(disc.join("SPLANET/VIDEO")).unwrap();
        fs::create_dir_all(disc.join("VOODOO")).unwrap();
        fs::write(disc.join("SPLANET/LEVEL.RWB"), b"original level").unwrap();
        fs::write(disc.join("SPLANET/VIDEO/INTRO.SMK"), b"video").unwrap();
        fs::write(disc.join("VOODOO/SPVOODOO.EXE"), b"exe").unwrap();
        fs::write(disc.join("VOODOO/RWDLVOOD.DLL"), b"renderware").unwrap();
        fs::write(disc.join("SPLANET/SPLANET.INI"), b"[scorch]\r\nfoo=bar\r\n").unwrap();
        let opts = InstallOptions {
            cd_root: disc,
            destination: root.path().join("Game"),
            language: 2,
        };
        (root, opts)
    }

    #[test]
    fn installs_using_original_folder_data() {
        let (_dir, opts) = fixture();
        let plan = plan_install(&opts).unwrap();
        let result = execute_install(&opts, &plan, |_, _, _| {}).unwrap();
        assert!(result.files_copied >= 4);
        assert_eq!(fs::read(opts.destination.join("LEVEL.RWB")).unwrap(), b"original level");
        assert_eq!(fs::read(opts.destination.join("SPVOODOO.EXE")).unwrap(), b"exe");
        assert!(opts.destination.join("VIDEO/INTRO.SMK").exists());
        let ini = fs::read_to_string(opts.destination.join("SPLANET.INI")).unwrap();
        assert!(ini.contains("language=2"));
        assert!(ini.contains("foo=bar"));
        assert!(ini.contains("video="));
    }

    #[test]
    fn refuses_to_overwrite_a_game_folder() {
        let (_dir, opts) = fixture();
        fs::create_dir_all(&opts.destination).unwrap();
        assert!(plan_install(&opts).is_err());
    }

    #[test]
    fn refuses_missing_voodoo_executable() {
        let (_dir, opts) = fixture();
        fs::remove_file(opts.cd_root.join("VOODOO/SPVOODOO.EXE")).unwrap();
        assert!(plan_install(&opts).is_err());
    }
}
