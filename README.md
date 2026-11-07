# ScorchedPlanetFix

**Play *Scorched Planet* (1996) on Windows and Linux (Wine / Proton) using an integrated open-source Glide renderer.**

[![Windows CI](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml/badge.svg)](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-blue.svg)](LICENSE)

ScorchedPlanetFix is a Windows x64 console installer, patcher, verifier and launcher for the original 32-bit game. It installs from your own ISO or CUE/BIN image, applies exact-edition FadeLow and No-CD patches, checks original assets with SHA-256 and launches the game.

## Version 2.0 — GameFrame only

**The only supported Glide wrapper is GameFrame Glide → OpenGL.** The Windows x64 launcher embeds a verified Windows x86 `glide.dll` built from pinned, Apache-2.0 [GameFrame source](https://github.com/tanepiper/game-frame), with Scorched-specific fixes.

- **No nGlide support**, no proprietary wrapper detection, download, copying, configuration or fallback.
- One borderless window using the **existing desktop resolution** (no display-mode switching), with 4:3 image centered and not stretched.
- 60 FPS limiter and keyboard/mouse input in the same game window.
- Automatic ISO/CUE installation, verification, patching and launching; second launches use the existing `splanet/` directory without re-extraction.
- Original game, disc image and saves are not modified or redistributed; previous original EXE backed up and immutable assets checked by SHA-256.
- Single `ScorchedPlanetFix.log` next to the launcher, including GameFrame diagnostics.
- Unknown external `glide.dll` files are **never overwritten** and are **not supported**. Recognized previous GameFrame experimental builds can be upgraded with a backup.

**Breaking changes:** nGlide and arbitrary third-party Glide DLLs are unsupported. The obsolete `--3dfx-logo` (nGlide-specific) and `--launch` switches are rejected.

> **Release candidate:** This branch builds **v2.0.0-rc.1** for Windows/Wine acceptance testing. The [latest published stable release](https://github.com/legluondunet/ScorchedPlanetFix/releases/latest) remains **v1.1.0**. GitHub Actions RC artifacts are for testing only; no stable v2 tag or release has been published.

## Supported disc editions

| Edition | Languages | FadeLow | No-CD |
| --- | --- | :---: | :---: |
| US InstallKey 1.08 | English | ✅ | ✅ |
| Europe InstallKey 1.06 | English, Spanish, Italian | ✅ | ✅ |
| Europe InstallKey 1.06 | French, German | ✅ | ✅ |
| Europe alternate FR/DE ISO InstallKey 1.06 | French, German | ✅ | ✅ |

All executable changes are specific to verified original SHA-256 fingerprints. See [edition support](docs/cd-editions.md).

## Installation and launch

Use the **v2 development build** from [Windows CI](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml), or a v2 release once published. Put `ScorchedPlanetFix.exe` beside a supported ISO or CUE/BIN image, then launch it.

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

If an existing `splanet/glide.dll` comes from nGlide or another external wrapper, **manually move that DLL to a backup outside `splanet/` before running v2**. ScorchedPlanetFix never overwrites it. The next normal launch checks the immutable game data before installing its own bundled GameFrame. An unrecognized existing DLL results in an explicit error and no modification.

### Platforms

The combined GameFrame build was validated by the maintainer on **Windows 11, Wine-staging and GE-Proton**, including installation, Alt+Tab, videos, gameplay and a second launch. The v2-only cleanup and packaging require separate CI validation.

On Linux, use a Wine/Proton environment with working **32-bit OpenGL graphics drivers**:

```bash
wine ScorchedPlanetFix.exe
```

Windows DPI scaling may make logical desktop dimensions different from physical panel pixels; GameFrame sizes its window to the current logical desktop without changing the display mode or refresh rate.

## License and documentation

ScorchedPlanetFix: **GPL-3.0-or-later** ([LICENSE](LICENSE)). Bundled GameFrame: **Apache-2.0** (license included in the Windows distribution). No original game files or third-party proprietary drivers are shipped. Independent community compatibility project maintained by **legluondunet assisted by ChatGPT (OpenAI)**.

[Final RC test plan (Windows/Wine)](docs/rc-test-plan-v2.0.0.md) · [v2.0.0 release notes](docs/release-notes-v2.0.0.md) · [Windows installer and migration](docs/windows-cli-installer.md) · [GameFrame implementation](docs/gameframe-experiment.md) · [v1.1.0 historical notes](docs/release-notes-v1.1.0.md)
