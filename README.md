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
| Europe (alternate FR/DE ISO, InstallKey 1.06) | French, German | ✅ | ✅ | ✅ |

Edits are strictly tied to SHA-256 fingerprints of the original executable and core game resources; unknown binaries are refused. The numbers above come from each disc's *InstallKey*, not independently verified executable version metadata. The checkmarks reflect owner-tested functionality, not exhaustive hardware or multi-mission compatibility. The alternate FR/DE ISO was validated under Wine/GE-Proton (extraction, FadeLow, No-CD, videos, menus, gameplay) on 2026-10-31. It differs from the previously supported FR/DE disc in five executables; all other 571 original file records match. See the [CD inventory notes](docs/cd-editions.md).

## Quick start

### 1. Get ScorchedPlanetFix

Download the **[latest published ScorchedPlanetFix release](https://github.com/legluondunet/ScorchedPlanetFix/releases/latest)** and extract its `ScorchedPlanetFix-v<version>-Windows-x64.zip`. Versioned downloads are listed on the Releases page; **v1.0.0** remains available for users who need the original release. The four-variant support and optional 3dfx logo are included starting with **v1.1.0**.

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

If several ISO/CUE images are present, the launcher refuses to guess. Specify the desired image explicitly on the command line. If `splanet/` already exists, it has priority over all disc images.

### 3. Install, verify and play

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

Running without arguments performs the complete workflow automatically: if `splanet/` already exists **beside the patcher**, it verifies the patched executable, original backup, all immutable game assets and local Glide DLL **without changing files**, then starts the game. Otherwise it searches for exactly one ISO/CUE beside the patcher, installs the game, verifies everything and starts playing. The ISO and save files are never overwritten. If an existing installation is unknown, incomplete or corrupted, the game is **not** launched and no CD fallback overwrites it. Relative explicit image paths are resolved beside the patcher, even if it was launched from another working directory.

### 4. Optional diagnostics and installation-only mode

```powershell
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --install-only
```

- **Normal execution** installs as necessary, performs the complete SHA-256 verification once, then starts the game and waits for it to exit.
- **`--verify`** checks the patched EXE, original backup, immutable game assets, INI and local Glide DLL without making changes or launching.
- **`--install-only`** installs without playing, or explicitly performs a recognized legacy migration (including known unused EXE cleanup and locally available nGlide setup). It is also useful after you install nGlide to finish an installation that previously lacked `glide.dll`.
- **`ScorchedPlanetFix.log`** records diagnostics and hardware information beside the patcher.

For safety, normal execution **does not attempt to repair an invalid existing installation**. If a legacy FadeLow-only version needs upgrading, run `--install-only` and then run the executable normally. Migration requires the corresponding original executable fingerprint and intact matching backup; unknown or modified executables are refused. Original saves remain untouched.

### Linux / Wine

Use the same Wine prefix for the patcher and nGlide. You may launch the executable from another working directory; its own folder determines where `splanet/` and the ISO/CUE are searched:


```bash
wine ScorchedPlanetFix.exe
wine ScorchedPlanetFix.exe --verify
wine ScorchedPlanetFix.exe --install-only
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

nGlide is proprietary freeware and **is not bundled or downloaded** by ScorchedPlanetFix. The patcher may copy your locally installed 32-bit `glide.dll` from a supported Windows/Wine location. It checks the DLL's PE32 format, which is not proof of the publisher's identity. Missing nGlide is reported by `--verify` and automatic play. The CD can still be installed using `--install-only`, then completed once the user has installed nGlide.

The launcher applies process-local settings: Vulkan when the loader is available (otherwise auto), desktop resolution, aspect correction, 60 Hz, VSync on, gamma 1.0 and **3dfx splash screen off by default**. To enable the optional 3dfx startup logo while running the automatic workflow, use `--3dfx-logo`:

Windows PowerShell:

```powershell
.\ScorchedPlanetFix.exe --3dfx-logo
```

Linux / Wine:

```bash
wine ScorchedPlanetFix.exe --3dfx-logo
```

Without `--3dfx-logo`, the splash logo remains disabled. **The logo requires the separate 32-bit `3DfxSpl.dll` plugin for `glide.dll`** (not `3DfxSpl2.dll`). Copy your own nGlide `3DfxSpl.dll` manually into the game's `splanet/` directory, next to `glide.dll` and `spvoodoo.exe`. When `--3dfx-logo` is requested, the launcher checks this local file read-only; if missing or invalid, it exits with a clear message instructing you to copy it. **There is no automatic copy or download.** Normal play and `--verify` do not need or inspect this optional file, which is excluded from the immutable file manifest. Global nGlide registry settings remain unchanged. Vulkan-loader detection alone does not guarantee graphics-driver compatibility.

## Documentation

- [Supported editions and original media fingerprints](docs/cd-editions.md)
- [Installation, verification and safe upgrades](docs/windows-cli-installer.md)
- [FadeLow and CD-check reverse-engineering research](docs/european-fadelow-research.md)
- [v1.1.0 release notes](docs/release-notes-v1.1.0.md)
- [v1.0.0 release notes](docs/release-notes-v1.0.0.md)

## License & credits

**GPL-3.0-or-later** — see [LICENSE](LICENSE).

Created by **legluondunet**, assisted by **ChatGPT (OpenAI)**.

*Scorched Planet* and its original content remain the property of their respective rights holders. Independent community compatibility project, unaffiliated with the original game publisher/developer or Zeus Software.