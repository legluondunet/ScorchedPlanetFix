# ScorchedPlanetFix

> **Development branch (PR #36):** ScorchedPlanetFix 2.1 is migrating
> to the CrimsonSkiesFix-style layout with ONE `game/` directory,
> `ScorchedPlanetFix.ini` and logs beside the launcher, fix-owned
> `state/` and `docs/`, and renderer DLLs alongside the game's EXE.
> [Read the proposed v2.1 layout](docs/unified-layout.md). This is
> experimental and **not yet the published v2.0.2 release**.
>

**Play *Scorched Planet* (1996) on Windows and Linux (Wine / Proton) using an integrated open-source Glide renderer.**

[![Windows CI](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml/badge.svg)](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-blue.svg)](LICENSE)

ScorchedPlanetFix is a Windows x64 console installer, patcher, verifier and launcher for the original 32-bit game. It installs from your own ISO or CUE/BIN image, applies exact-edition FadeLow and No-CD patches, checks original assets with SHA-256 and launches the game.

## Version 2.0.2 — GameFrame only

**The only supported Glide wrapper is GameFrame Glide → OpenGL.** The Windows x64 launcher embeds a verified Windows x86 `glide.dll` built from pinned, Apache-2.0 [GameFrame source](https://github.com/tanepiper/game-frame), with Scorched-specific fixes.

- **No nGlide support**, no proprietary wrapper detection, download, copying, configuration or fallback.
- One borderless window using the **existing desktop resolution** (no display-mode switching), with 4:3 image centered and not stretched.
- 60 FPS limiter and keyboard/mouse input in the same game window.
- Automatic ISO/CUE installation, verification, patching and launching; second launches use the existing `splanet/` directory without re-extraction.
- The v2.0.2 release provides `--launch-no-verify` to start `splanet/spvoodoo.exe` directly without checking any installed game or Glide DLL files. This is an explicit diagnostic opt-out, not the default.
- Your original CD image is never modified or redistributed; the installer patches a verified copy of the game executable and preserves the exact original in `.bak`. Save files remain writable for normal gameplay, and immutable game assets are checked by SHA-256.
- Two append-only logs beside the launcher: `ScorchedPlanetFix_installer.log` for installation, verification and launching, and `ScorchedPlanetFix.log` for the GameFrame DLL. Each session has a shared `session_id`.
- v2.0.1 adds a [compact red version-commit signature](docs/main-menu-version-probe.md) at the bottom-right of the game's original root menu. It persists when hovering buttons or opening the Language, Quit and New Game panels; it disappears in videos and gameplay.
- Older or unknown existing `glide.dll` files are **not loaded**. On normal launch, after the immutable game-file audit, the original file is renamed to `glide.dll.bak` (replacing an existing regular `.bak`) and the verified bundled GameFrame DLL is installed automatically. Only one managed backup is kept; the replacement and hashes are logged. Symlinks/non-files are rejected.

**Breaking changes:** nGlide and arbitrary third-party Glide DLLs are unsupported. The obsolete `--3dfx-logo` (nGlide-specific) and `--launch` switches are rejected.

> **Release history:** v2.0.2 adds separate installer/GameFrame logs, `--launch-no-verify`, read-only `--inspect` and `--verbose`. v2.0.1 includes the new version signature, automatic Glide DLL migration and safer installation checks. The [v2.0.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v2.0.0) is preserved with its original behavior (external `glide.dll` must be moved aside manually). The [v1.1.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.1.0) requires nGlide. GitHub Actions artifacts are temporary development builds; use the [latest published release](https://github.com/legluondunet/ScorchedPlanetFix/releases/latest) for a permanent download.

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
```

The first launch extracts and patches the image in a private staging directory, installs GameFrame from the EXE, verifies the installation and starts the game. Later starts detect `splanet/`, verify the edition and its immutable data, then play without a disc image. Unknown or corrupted game installations are refused and never overwritten.

Additional commands:

```powershell
.\ScorchedPlanetFix.exe --install-only
.\ScorchedPlanetFix.exe --launch-no-verify
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --inspect image.iso
.\ScorchedPlanetFix.exe --inspect image.cue image.bin
.\ScorchedPlanetFix.exe --inspect image.cue image.img
.\ScorchedPlanetFix.exe --verbose
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe -h
.\ScorchedPlanetFix.exe --version
```

- `--inspect`: read-only unknown ISO/CUE file inventory, optional separate BIN/IMG
  data track. Writes a separate `ScorchedPlanetFix_inspect.log` beside the
  launcher without installing or running the game. Positional CD images
  without `--inspect` are no longer accepted.
- `--verbose`: detailed diagnostics, combinable with other commands.
- `--help` and `-h`: same help.
- `--verify`: **read-only** audit of the game, original backup, patch, INI and exact GameFrame DLL. Does not launch.
- `--launch-no-verify` (**v2.0.2**): launch the expected installed `splanet/spvoodoo.exe` directly, bypassing all game file existence, integrity, and Glide DLL checks. Never installs, repairs, patches, or replaces files; if the executable is missing, the OS reports a launch failure. Use only with trusted local game files and DLLs.
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

[v2.0.2 release notes](docs/release-notes-v2.0.2.md) · [v2.0.1 historical notes](docs/release-notes-v2.0.1.md) · [Pre-release audit checklist](docs/pre-release-audit-post24.md) · [Menu signature implementation](docs/main-menu-version-probe.md) · [v2.0.0 release notes (historical)](docs/release-notes-v2.0.0.md) · [Windows installer and migration](docs/windows-cli-installer.md) · [GameFrame implementation](docs/gameframe-experiment.md) · [v1.1.0 historical notes](docs/release-notes-v1.1.0.md)

### Development branch — native Matrox configuration (19 November 2026)

The experimental PR #36 now uses the player's tested English/maximum-quality
Matrox native `splanet.ini` defaults. Automatic MSI95 BMP screenshots are
disabled; manual F12 remains available. Root `ScorchedPlanetFix.ini`
now places `FPSLimit=60` in `[Graphics]` and adds a disabled
`[Cheats] UnlimitedFuel=0` placeholder. The native fuel cheat is **not yet
implemented** and an enabled option is rejected, rather than silently
pretending to work.

### Native in-game cheats (PR #36, experimental)

`[Cheats]` now lists `ALIAH=0` (invulnerability) and
`FATAL=0` (all weapons and ammunition), alongside `UnlimitedFuel=0`.
The native codes are typed during gameplay. Automated activation via INI
is not yet proven and enabling either option returns an explicit
unsupported-feature error. Some third-party lists use `ALLAH`
instead of `ALIAH` for invulnerability; the project preserves the
user-requested `ALIAH` spelling until game testing resolves the ambiguity.

### Debug logs and preserved OEM reference binaries (2026-11-20)

A fresh root `ScorchedPlanetFix.ini` now includes human-readable comments
for every option and `[Debug] Log=0`. Use `Log=1` to enable a new
noncumulative runtime log for GameFrame or Matrox. The installer's own
noncumulative `ScorchedPlanetFix_installer.log` remains available in either
mode. Matrox's `splanetw.exe.bak` and `msi95.dll.bak` are exact, audited
OEM originals retained for safe original-content verification; they are never
used to launch or render. Deleting them invalidates the current verifier.

### First-run diagnostic collection (2026-11-20)

On a clean installation the first verified game launch produces BOTH
`ScorchedPlanetFix_installer.log` and `ScorchedPlanetFix.log`, even with
the default `[Debug] Log=0`. After the first normal game exit the
launcher records the completion in `state/`. Normal subsequent
`Log=0` play keeps the original runtime diagnostic file without
updating it. `Log=1` allows a fresh log per game session.
The root INI never changes its `Log=0` default automatically.

### Multiple Matrox screenshots without overwriting (2026-11-20)

Every separate **F12** keypress on the Matrox edition generates a unique
numbered pair in `game/`: `matrox_capture_0001_frame.bmp` and
`matrox_capture_0001_coverage.bmp`, then `0002`, `0003`, etc.
Existing screenshots are preserved across launches and the files are
opened with the Windows no-overwrite `CREATE_NEW` flag. Release F12
between captures. Attach both files with matching numbers when reporting
the menu transparency and world rendering issues.

### Automatic update of the installer-owned Matrox MSI95 DLL (2026-11-20)

A prior clean-room MSI95.dll may come from any experimental PR #36 build,
and its SHA-256 necessarily differs when the source or compilation changes.
The launcher now authenticates **all 66 OEM reference files**, including the
original proprietary driver `game/msi95.dll.bak`, the original executable
`game/splanetw.exe.bak`, and the hash-locked patched executable, *before*
automatically replacing a different/missing `game/msi95.dll` with the DLL
embedded in the new ScorchedPlanetFix.exe. A temporary staging/rollback
transaction protects against errors; no new persistent `.bak` is made for
the launcher-owned DLL. All assets, saves and native INI values are preserved.

A normal launch or `--install-only` updates the driver as needed. The
read-only `--verify` mode still checks without modifying anything, and
`--launch-no-verify` retains its existing unchecked semantics. Runtime
messages explicitly record the old/new DLL hashes when an upgrade occurs.

### Matrox msi95.dll ownership rule (2026-11-20)

`game/msi95.dll` is exclusively an installer-managed rendering DLL.
On every verified Matrox prepare/launch (or `--install-only`), after
authenticating the original OEM game files and the patched EXE, the launcher
compares its SHA-256 with the DLL embedded in the running installer:

- **Identical:** keep the existing file byte-for-byte without rewriting it.
- **Different, whether recognized or not:** replace it automatically with
  the verified embedded DLL. No legacy SHA-256 allowlist and no user action.
- **Missing:** restore the embedded DLL.
- A temporary rollback protects against failed updates; it is removed after
  successful verification. No persistent backup for launcher-supplied DLLs.
- The *separate* `game/msi95.dll.bak` remains the original proprietary
  Matrox driver, authenticated and never overwritten.
- A symlink or non-regular file is rejected rather than followed for safety.

The explicit read-only `--verify` mode never performs replacements.
