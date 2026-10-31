# ScorchedPlanetFix

**Play *Scorched Planet* (1996) on modern systems — from your own CD image.**

[![Windows x64 CI](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml/badge.svg)](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml)
[![Linux Wine / Proton compatible](https://img.shields.io/badge/Linux%20Wine%20%2F%20Proton-compatible-428b40?logo=linux&logoColor=white)](#linux--wine)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-blue.svg)](LICENSE)

ScorchedPlanetFix is a **free and open-source console installer, patcher, verifier and launcher** for Criterion's *Scorched Planet* (1996).

It installs from your own **ISO or CUE/BIN**, applies edition-specific **FadeLow** and **No-CD** fixes, and launches the game using your separately installed **nGlide** wrapper. No original game files or proprietary drivers are distributed.

- **Windows x64** program; the original game remains **32-bit**
- Also runs under **Wine on Linux**
- **No GUI; no CD mounted at play time** for the tested editions
- SHA-256 checks and an original-executable backup safeguard the patches

> [!IMPORTANT]
> You need your **own copy of Scorched Planet** and **nGlide**. This project does not include the game, disc images or any nGlide binaries. The owner has now tested native Windows 11 installation and gameplay; broader hardware and mission compatibility remains unverified.

## Supported CD editions

| Edition | Available languages | FadeLow | No-CD | CUE/BIN |
| --- | --- | :---: | :---: | :---: |
| US (InstallKey 1.08) | English | ✅ | ✅ | ✅ |
| Europe (InstallKey 1.06) | English, Spanish, Italian | ✅ | ✅ | ✅ |
| Europe (InstallKey 1.06) | French, German | ✅ | ✅ | ✅ |

Edits are strictly tied to SHA-256 fingerprints of the original executable and core game resources; unknown binaries are refused. The numbers above come from each disc's *InstallKey*, not independently verified executable version metadata. The checks reflect owner-reported testing under Wine and native Windows 11, not every mission or hardware configuration.

## Quick start

### 1. Get ScorchedPlanetFix

Download the **[ScorchedPlanetFix v1.0.0 Windows x64 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.0.0)** and extract `ScorchedPlanetFix-v1.0.0-Windows-x64.zip`.

> The release includes a separate `.zip.sha256` checksum file. Verify your downloaded ZIP against it before extracting. GitHub Actions artifacts are development builds, not permanent release downloads.

### 2. Provide the disc image and nGlide

1. Place `ScorchedPlanetFix.exe` in a folder with **one** supported disc image: an ISO, or a CUE and its BIN.
2. [Install nGlide from Zeus Software](https://www.zeus-software.com/downloads/nglide) in the same Windows/Wine environment, **or** place your own 32-bit nGlide `glide.dll` next to the patcher.
3. Open a terminal in that folder.

Example:

```text
ScorchedPlanetFix/
├── ScorchedPlanetFix.exe
├── Scorched Planet.cue
├── Scorched Planet.bin
└── glide.dll                  # Only if supplied manually
```

If both ISO and CUE media are present, choose the image explicitly on the command line.

### 3. Install

In a Windows PowerShell terminal:

```powershell
.\ScorchedPlanetFix.exe
```

Or specify your own disc image:

```powershell
.\ScorchedPlanetFix.exe "Scorched Planet.iso"
.\ScorchedPlanetFix.exe "Scorched Planet.cue"
.\ScorchedPlanetFix.exe "Scorched Planet.cue" "Scorched Planet.bin"
```

The installer creates `splanet/`, copies game files, preserves the original `spvoodoo.exe.bak`, patches the detected edition and creates a portable `splanet.ini`. Existing game installations and save files are not overwritten.

### 4. Verify and launch

```powershell
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --launch
```

- **`--verify`** checks the patched EXE, original backup, immutable game assets, INI and local nGlide DLL.
- **`--launch`** performs the same verification before launching the game and monitors its exit.
- **`ScorchedPlanetFix.log`** contains diagnostic messages next to the patcher.

To update a supported existing installation, run the latest patcher **without arguments** from the directory containing `splanet/`. Migration from older FadeLow-only patches requires an intact, checksum-verified original `.bak` and keeps your saves.

### Linux / Wine

Use the same working directory and Wine prefix for the patcher and nGlide:


```bash
wine ScorchedPlanetFix.exe
wine ScorchedPlanetFix.exe --verify
wine ScorchedPlanetFix.exe --launch
```

There is no graphical interface. Use `--help` to see all commands.

## What does it fix?

**FadeLow** removes excessively long fade transitions on modern systems. The edition-specific **No-CD** fix avoids the original game's media-check failure when running from an installed copy. For each supported executable, ScorchedPlanetFix changes exactly **six bytes**: four for FadeLow and two for the CD check.

The installer also:

- Reads supported ISO and CUE/BIN layouts; keeps your original image untouched
- Copies the needed videos and language files and normalizes names
- Omits the unused `splanet.exe` and `splanetw.exe` software-renderer executables and obsolete 3dfx drivers
- Uses the portable `video= video` INI setting for local video playback
- Excludes save slots and mutable settings from immutable-file hash enforcement
- Sets nGlide options only for the launched game, never through global registry changes

## nGlide (required, separate download)

**[Official nGlide download — Zeus Software](https://www.zeus-software.com/downloads/nglide)**

nGlide is proprietary freeware and **is not bundled or downloaded** by ScorchedPlanetFix. The patcher may copy your locally installed 32-bit `glide.dll` from a supported Windows/Wine location. It checks the DLL's PE32 format, which is not proof of the publisher's identity. Missing nGlide is reported by `--verify` and `--launch`.

The launcher applies process-local settings: Vulkan when the loader is available (otherwise auto), desktop resolution, aspect correction, 60 Hz, VSync on, gamma 1.0 and **3dfx splash screen off by default**. To enable the optional 3dfx startup logo, use `--launch --3dfx-logo`:

Windows PowerShell:

```powershell
.\ScorchedPlanetFix.exe --launch --3dfx-logo
```

Linux / Wine:

```bash
wine ScorchedPlanetFix.exe --launch --3dfx-logo
```

Without `--3dfx-logo`, the splash logo remains disabled. **The logo requires the separate 32-bit `3DfxSpl.dll` plugin for `glide.dll`** (not `3DfxSpl2.dll`). When the flag is selected, the launcher checks for the DLL beside the game, or copies the user's locally installed DLL from beside ScorchedPlanetFix.exe or the Windows/Wine `SysWOW64`/`System32` directory. If the plugin is missing or invalid, launch stops with an explicit diagnostic instead of silently omitting the logo. It does not download or redistribute the plugin; the game files and global nGlide registry settings are otherwise unchanged. The optional logo may not work correctly with every nGlide version or graphics setup. Vulkan-loader detection alone does not guarantee graphics-driver compatibility.

## Documentation

- [Supported editions and original media fingerprints](docs/cd-editions.md)
- [Installation, verification and safe upgrades](docs/windows-cli-installer.md)
- [FadeLow and CD-check reverse-engineering research](docs/european-fadelow-research.md)
- [Draft v1.0.0 release notes](docs/release-notes-v1.0.0.md)

## License & credits

**GPL-3.0-or-later** — see [LICENSE](LICENSE).

Created by **legluondunet**, assisted by **ChatGPT (OpenAI)**.

*Scorched Planet* and its original content remain the property of their respective rights holders. Independent community compatibility project, unaffiliated with the original game publisher/developer or Zeus Software.