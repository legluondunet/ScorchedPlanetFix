# ScorchedPlanetFix

**Windows x64 command-line installer, verifier, patcher and launcher for Scorched Planet (1996).**

Version: **1.0.0 + build commit**. Author: legluondunet, assisted by ChatGPT (OpenAI). License: **GPL-3.0-or-later**.

ScorchedPlanetFix runs as a **console executable, without a GUI**, on Windows 64-bit and in a compatible 64-bit Wine environment on Linux. The original game remains a **32-bit** program. No commercial game assets, disc images, drivers, proprietary Glide DLLs or nGlide installers are distributed.

## Commands

```powershell
.\ScorchedPlanetFix.exe
.\ScorchedPlanetFix.exe --nogui
.\ScorchedPlanetFix.exe --nogui "D:\CD\Scorched Planet.iso"
.\ScorchedPlanetFix.exe --nogui "D:\CD\Scorched Planet.cue"
.\ScorchedPlanetFix.exe --nogui "D:\CD\Scorched Planet.cue" "D:\CD\Scorched Planet.bin"
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --launch
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe --version
```

**No arguments** means install/patch in the **current working directory**. `--nogui` remains an alias for backwards compatibility, since the program is now always console-only. With no explicit ISO/CUE, find a single supported `.iso` or `.cue` in the working directory. A CUE references the associated BIN, or the BIN may be supplied as the second operand. Do not overwrite an existing `splanet` directory; if already installed, verify/patch its recognized original EXE and normalize its INI.

**`--verify`** always inspects `splanet` **beside ScorchedPlanetFix.exe** (not necessarily your shell's working directory), checks the edition-specific patched executable, the original `.bak`, the portable INI and each immutable game asset against the owner's SHA-256 inventory; save games (`*.sav`) and custom settings are mutable and excluded from hash enforcement. It also checks for a **local 32-bit `splanet/glide.dll`**.

**`--launch`** verifies **before each launch**, and only then starts `spvoodoo.exe` in the `splanet` working directory. It waits for the game to exit and reports failures. **`--help`** shows commands, paths, nGlide notes and the version, without requiring an installation; `--version` prints the version only.

All errors are shown in the terminal and logged to `ScorchedPlanetFix.log`. Startup errors attempt a fallback log if the primary location is inaccessible.

## ISO and CUE/BIN installation

Supports three exact owner-inventoried CD editions: **US**, **Europe EN/ES/IT**, and **Europe FR/DE**, identified from SHA-256 hashes of the original `VOODOO/SPVOODOO.EXE`, `VOODOO/RWDLVOOD.DLL` and `SPLANET/GEOMETRY.RWB`. No proprietary assets are bundled or downloaded.

The program reads ISO9660/Joliet (and supported UDF images), and supported CUE first-track data formats `MODE1/2048`, `MODE1/2352` and XA Form 1 `MODE2/2352`. CUE/BIN is converted to a temporary data-only ISO, then passed through the same parser. The owner confirms **these Scorched Planet CD editions contain no CD audio tracks**.

The installer extracts only original `SPLANET/` and `VOODOO/` into separate temporary trees, copies all `SPLANET/` files into a staged `splanet` directory, and only `spvoodoo.exe` and `rwdlvood.dll` from `VOODOO/`. It excludes obsolete `glide.dll`, `sst1init.dll` and `fxmemmap.vxd` from the game disc and skips the unused software-renderer executable `SPLANET/SPLANETW.EXE` entirely. When updating an existing installation, it removes that obsolete executable only if its SHA-256 matches the original CD edition; unknown or altered files are never deleted. `--verify` refuses an installation still containing `splanetw.exe` until safely cleaned with the no-argument installer. File and directory names are normalized to lowercase with collision rejection and SHA-256 checks.

Before modifying the original executable, it saves `splanet/spvoodoo.exe.bak`, then automatically applies the exact edition-specific correction: **US FadeLow + experimental CD-media gate (6 bytes; gameplay retest required)**, EN/ES/IT FadeLow only, or FR/DE FadeLow + the previously user-tested CD-media gate. The US candidate is checksum-gated and must pass a new real no-disc test before release. Existing US FadeLow-only installs are safely upgraded from their verified original `.bak` by rerunning the installer with no arguments. It subsequently generates `splanet.ini` with `[scorch]` and **`video= video`** (the owner's validated portable value), appropriate default `language=`, and `HighQualityVideo=1`. On failure the staged installation is discarded; original disc images and pre-existing installations are not overwritten.

## nGlide — separate proprietary dependency

nGlide is **freeware, not GPL/open source**, owned by Zeus Software. Its distribution permission for third-party bundles is not established. **It is NOT included or downloaded** by ScorchedPlanetFix.

**Official download:** https://www.zeus-software.com/downloads/nglide

Download and install nGlide yourself in Windows/the same Wine prefix. ScorchedPlanetFix can copy the **locally installed 32-bit `glide.dll`** from Windows `SysWOW64` (or a copy you have personally supplied next to ScorchedPlanetFix.exe) into `splanet`, without modifying the source. It checks that the DLL has a valid 32-bit Windows PE header, though this alone does not establish publisher authenticity. If the DLL is not available, installation may finish but verification and launch will report exactly what is missing.

For the game process **only**, nGlide receives the following documented environment variables (overriding its registry configuration, which is left untouched):

| Setting | Value |
| --- | --- |
| Backend | Vulkan when the Vulkan loader is available (`NGLIDE_BACKEND=2`), otherwise automatic (`0`) |
| Resolution | Desktop (`NGLIDE_RESOLUTION=1`) |
| Aspect ratio | Aspect correction (`NGLIDE_ASPECT=1`) |
| Refresh | 60 Hz (`NGLIDE_REFRESH=2`) |
| Vertical sync | On (`NGLIDE_VSYNC=1`) |
| Gamma | 1.0 (`NGLIDE_GAMMA=5`) |
| 3dfx splash | Off (`NGLIDE_SPLASH=0`) |

These settings are documented by [Zeus Software](https://www.zeus-software.com/forum/viewtopic.php?p=4254). They avoid interfering with nGlide settings for other games. Vulkan-loader detection does **not guarantee** the GPU driver is fully Vulkan-compatible; if Vulkan fails, troubleshooting may require choosing another backend in nGlide's own configurator.

## Development and validation

Read [CD edition profiles](docs/cd-editions.md) and [CLI install/verify notes](docs/windows-cli-installer.md). No PR is merged without the owner's approval. Tests in GitHub Actions validate Windows x64 compilation and executable type; real-media installation and runtime tests on Windows and Wine are also required before a stable release.
