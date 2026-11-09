# ScorchedPlanetFix

**Play *Scorched Planet* (1996) on Windows and Linux (Wine / Proton) using an integrated open-source Glide renderer.**

[![Windows CI](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml/badge.svg)](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-blue.svg)](LICENSE)

ScorchedPlanetFix is a Windows x64 console installer, patcher, verifier and launcher for the original 32-bit game. It installs from your own ISO or CUE/BIN image, applies exact-edition FadeLow and No-CD patches, checks original assets with SHA-256 and launches the game.

## Version 2.0.1 — GameFrame only

**The only supported Glide wrapper is GameFrame Glide → OpenGL.** The Windows x64 launcher embeds a verified Windows x86 `glide.dll` built from pinned, Apache-2.0 [GameFrame source](https://github.com/tanepiper/game-frame), with Scorched-specific fixes.

- **No nGlide support**, no proprietary wrapper detection, download, copying, configuration or fallback.
- One borderless window using the **existing desktop resolution** (no display-mode switching), with 4:3 image centered and not stretched.
- 60 FPS limiter and keyboard/mouse input in the same game window.
- Automatic ISO/CUE installation, verification, patching and launching; second launches use the existing `splanet/` directory without re-extraction.
- Your original CD image is never modified or redistributed; the installer patches a verified copy of the game executable and preserves the exact original in `.bak`. Save files remain writable for normal gameplay, and immutable game assets are checked by SHA-256.
- Single `ScorchedPlanetFix.log` next to the launcher, including GameFrame diagnostics.
- v2.0.1 adds a [compact red version-commit signature](docs/main-menu-version-probe.md) at the bottom-right of the game's original root menu. It persists when hovering buttons or opening the Language, Quit and New Game panels; it disappears in videos and gameplay.
- Older or unknown existing `glide.dll` files are **not loaded**. On normal launch, after the immutable game-file audit, the original file is renamed to `glide.dll.bak` (replacing an existing regular `.bak`) and the verified bundled GameFrame DLL is installed automatically. Only one managed backup is kept; the replacement and hashes are logged. Symlinks/non-files are rejected.

**Breaking changes:** nGlide and arbitrary third-party Glide DLLs are unsupported. The obsolete `--3dfx-logo` (nGlide-specific) and `--launch` switches are rejected.

> **Release history:** v2.0.1 includes the new version signature, automatic Glide DLL migration and safer installation checks. The [v2.0.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v2.0.0) is preserved with its original behavior (external `glide.dll` must be moved aside manually). The [v1.1.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.1.0) requires nGlide. GitHub Actions artifacts are temporary development builds; use the [latest published release](https://github.com/legluondunet/ScorchedPlanetFix/releases/latest) for a permanent download.

## Supported disc editions

| Edition | Languages | FadeLow | No-CD |
| --- | --- | :---: | :---: |
| US InstallKey 1.08 | English | ✅ | ✅ |
| Europe InstallKey 1.06 | English, Spanish, Italian | ✅ | ✅ |
| Europe InstallKey 1.06 | French, German | ✅ | ✅ |
| Europe alternate FR/DE ISO InstallKey 1.06 | French, German | ✅ | ✅ |

All executable changes are specific to verified original SHA-256 fingerprints. See [edition support](docs/cd-editions.md).

## Installation and launch

Download the [latest published release](https://github.com/legluondunet/ScorchedPlanetFix/releases/latest). Development builds from [`main` CI](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml) are for testing. Place `ScorchedPlanetFix.exe` beside a supported ISO or CUE/BIN image, then launch it.

```powershell
.\ScorchedPlanetFix.exe
.\ScorchedPlanetFix.exe "Scorched Planet.iso"
.\ScorchedPlanetFix.exe "Scorched Planet.cue" "Scorched Planet.bin"
```

The first launch extracts and patches the image in a private staging directory, installs GameFrame from the EXE, verifies the installation and starts the game. Later starts detect `splanet/`, verify the edition and its immutable data, then play without a disc image. Unknown or corrupted game installations are refused and never overwritten.

Additional commands:

```powershell
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --install-only
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe --version
```

- `--verify`: **read-only** audit of the game, original backup, patch, INI and exact GameFrame DLL. Does not launch.
- `--install-only`: install without launching, or explicitly migrate a recognized previous game executable.
- No nGlide flags, registry changes or external wrapper setup.

### Moving from v1.1.0 / external Glide

If an existing `splanet/glide.dll` comes from nGlide, another external wrapper, or an older GameFrame build, **no manual DLL migration is necessary**. After validating the immutable game data, a normal launch renames that regular file to the fixed `glide.dll.bak`, **replacing any existing regular backup**, installs the verified bundled GameFrame and starts the game. It never creates new numbered/hash-suffixed backups. The previous backup's contents are intentionally discarded during replacement; the latest original DLL and both SHA-256 hashes are recorded in `ScorchedPlanetFix.log`. `--verify` stays strictly read-only; symlinks and non-regular DLL/backup paths remain unsupported.

### Platforms

The combined GameFrame build was validated by the maintainer on **Windows 11, Wine-staging and GE-Proton**. The v2.0.0 release candidate was separately exercised on native Windows 11 and Wine-staging 11.17, including installation, launch and verification. CI builds and checks the Windows x64 launcher and integrated x86 Glide DLL.

On Linux, use a Wine/Proton environment with working **32-bit OpenGL graphics drivers**:

```bash
wine ScorchedPlanetFix.exe
```

Windows DPI scaling may make logical desktop dimensions different from physical panel pixels; GameFrame sizes its window to the current logical desktop without changing the display mode or refresh rate.

## License and documentation

ScorchedPlanetFix: **GPL-3.0-or-later** ([LICENSE](LICENSE)). Bundled GameFrame: **Apache-2.0** (license included in the Windows distribution). No original game files or third-party proprietary drivers are shipped. Independent community compatibility project maintained by **legluondunet assisted by ChatGPT (OpenAI)**.

[v2.0.1 release notes](docs/release-notes-v2.0.1.md) · [Pre-release audit checklist](docs/pre-release-audit-post24.md) · [Menu signature implementation](docs/main-menu-version-probe.md) · [v2.0.0 release notes (historical)](docs/release-notes-v2.0.0.md) · [Windows installer and migration](docs/windows-cli-installer.md) · [GameFrame implementation](docs/gameframe-experiment.md) · [v1.1.0 historical notes](docs/release-notes-v1.1.0.md)
