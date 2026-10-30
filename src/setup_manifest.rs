//! Interpret language availability from the original InstallShield SETUP.INI.
//! Values English/Francais/Deutsch/Italiano/Espagnol are availability flags,
//! NOT verified values for the game's SPLANET.INI "language" setting.

use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CdLanguages {
    pub english: bool,
    pub french: bool,
    pub german: bool,
    pub italian: bool,
    pub spanish: bool,
    pub default_language: Option<u8>,
}

impl CdLanguages {
    /// Current setup UI offers English, French and German.
    /// Numeric codes in SPLANET.INI must still be verified with an installed game.
    pub fn offers(&self, code: u8) -> bool {
        match code {
            1 => self.english,
            2 => self.french,
            3 => self.german,
            _ => false,
        }
    }

    pub fn preferred_supported_code(&self) -> Option<u8> {
        // Favor the original default only when its code corresponds to a
        // selectable language. Other codes are kept as raw metadata.
        if let Some(code) = self.default_language {
            if self.offers(code) {
                return Some(code);
            }
        }
        [1, 2, 3].into_iter().find(|code| self.offers(*code))
    }
}

pub fn parse_cd_languages(content: &str) -> Result<CdLanguages, String> {
    let mut in_criterion = false;
    let mut section_found = false;
    let mut flags = [None; 5];
    let mut default_language = None;

    for raw_line in content.lines() {
        let line = raw_line.trim();
        if line.starts_with('[') && line.ends_with(']') {
            in_criterion = line[1..line.len() - 1].trim().eq_ignore_ascii_case("Criterion Studios");
            section_found |= in_criterion;
            continue;
        }
        if !in_criterion || line.starts_with(';') || line.starts_with('#') {
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        let key = key.trim();
        let value = value.trim();
        if key.eq_ignore_ascii_case("DefaultLanguage") {
            default_language = value.parse::<u8>().ok();
            continue;
        }
        let index = ["English", "Francais", "Deutsch", "Italiano", "Espagnol"]
            .iter()
            .position(|name| key.eq_ignore_ascii_case(name));
        if let Some(index) = index {
            flags[index] = Some(match value {
                "0" => false,
                "1" => true,
                _ => return Err(format!("Valeur de disponibilite invalide : {key}={value}")),
            });
        }
    }

    if !section_found {
        return Err("SETUP.INI ne contient pas la section [Criterion Studios].".into());
    }
    if flags.iter().any(Option::is_none) {
        return Err("SETUP.INI ne contient pas les cinq indicateurs de langue attendus.".into());
    }
    Ok(CdLanguages {
        english: flags[0].unwrap(),
        french: flags[1].unwrap(),
        german: flags[2].unwrap(),
        italian: flags[3].unwrap(),
        spanish: flags[4].unwrap(),
        default_language,
    })
}

/// Only read a candidate SETUP.INI from the user's own CD, never execute it.
/// Missing file is allowed for now (unverified editions remain in preview).
pub fn read_cd_languages(root: &Path) -> Result<Option<CdLanguages>, String> {
    let candidates: [PathBuf; 3] = [
        root.join("SETUP.INI"),
        root.join("INSTALL").join("SETUP.INI"),
        root.join("INSTALL").join("WINDOWS").join("SETUP.INI"),
    ];
    for path in candidates {
        if !path.is_file() {
            continue;
        }
        let metadata = fs::metadata(&path)
            .map_err(|e| format!("Impossible d'inspecter {} : {e}", path.display()))?;
        if metadata.len() > 128 * 1024 {
            return Err(format!("SETUP.INI trop volumineux : {}", path.display()));
        }
        let contents = fs::read(&path)
            .map_err(|e| format!("Impossible de lire {} : {e}", path.display()))?;
        // Read-only data, with lossy conversion for historic non-UTF-8 metadata.
        return parse_cd_languages(&String::from_utf8_lossy(&contents)).map(Some);
    }
    Ok(None)
}

#[cfg(test)]
mod tests {
    use super::*;

    const ORIGINAL: &str = "[Startup]\r\nAppName=Scorched Planet\r\n\
[Criterion Studios]\r\nVersion=1.06\r\nEnglish=1\r\nFrancais=0\r\nDeutsch=0\r\n\
Italiano=1\r\nEspagnol=1\r\nDefaultLanguage=1\r\n\
[Voodoo]\r\nWinAppFile=SPVOODOO.EXE\r\nMaxDetail=1\r\n";

    #[test]
    fn english_cd_only_offers_english_among_supported_languages() {
        let manifest = parse_cd_languages(ORIGINAL).unwrap();
        assert!(manifest.english);
        assert!(manifest.italian && manifest.spanish);
        assert!(!manifest.french && !manifest.german);
        assert_eq!(manifest.default_language, Some(1));
        assert_eq!(manifest.preferred_supported_code(), Some(1));
        assert!(manifest.offers(1));
        assert!(!manifest.offers(2));
        assert!(!manifest.offers(3));
    }

    #[test]
    fn french_german_cd_manifest_chooses_french() {
        let manifest = parse_cd_languages(
            "[Criterion Studios]\nEnglish=0\nFrancais=1\nDeutsch=1\nItaliano=0\nEspagnol=0\nDefaultLanguage=2"
        ).unwrap();
        assert_eq!(manifest.preferred_supported_code(), Some(2));
        assert!(!manifest.offers(1));
    }

    #[test]
    fn missing_ini_does_not_prevent_a_legacy_install() {
        let dir = tempfile::tempdir().unwrap();
        assert!(read_cd_languages(dir.path()).unwrap().is_none());
    }

    #[test]
    fn detects_ini_in_install_windows() {
        let dir = tempfile::tempdir().unwrap();
        let subdir = dir.path().join("INSTALL").join("WINDOWS");
        fs::create_dir_all(&subdir).unwrap();
        fs::write(subdir.join("SETUP.INI"), ORIGINAL).unwrap();
        assert!(read_cd_languages(dir.path()).unwrap().unwrap().english);
    }
}
