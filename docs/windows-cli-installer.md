# ScorchedPlanetFix Windows x64 CLI (post-v2.0.0 development)

This document covers the current **`main` development code after PR #24**. The [published v2.0.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v2.0.0) includes GameFrame, but does **not** yet contain the automatic backup of arbitrary pre-existing `glide.dll` described below. For the published behavior, consult its [historical release notes](release-notes-v2.0.0.md). The [v1.1.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.1.0) remains available for nGlide users.

## Commands

```powershell
.\ScorchedPlanetFix.exe
.\ScorchedPlanetFix.exe "D:\ISO\Scorched Planet.iso"
.\ScorchedPlanetFix.exe "D:\ISO\Scorched Planet.cue" "D:\ISO\Scorched Planet.bin"
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --install-only
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe --version
```

The executable searches for `splanet/` beside itself. If present, the game is verified before play; otherwise one unambiguous ISO/CUE image beside the launcher is extracted, patched, verified and launched. Explicit image paths are supported; unknown, damaged or incomplete installations are never silently overwritten. `--verify` is strictly read-only. `--install-only` performs an explicit installation or recognized older-edition migration without launching.

`--launch` and `--3dfx-logo` are unsupported and rejected.

## GameFrame Glide is the only supported renderer

The Windows x64 launcher embeds the Windows x86 Apache-2.0 GameFrame Glide DLL built from pinned source at commit `f195a074a63cc150673c38becdec696163b0b3d1`. The embedded DLL's PE32 architecture and SHA-256 are verified before installation and the exact expected SHA-256 is verified again before spawning the 32-bit game.

- nGlide and arbitrary external Glide DLLs are **unsupported**.
- No fallback from `SysWOW64`, external launcher directory or third-party paths.
- No `NGLIDE_*` environment variables, configurator settings or 3dfx splash DLL.
- At normal launch, after the immutable game-file audit, any existing **regular** `splanet/glide.dll` whose hash differs from the bundled DLL is automatically renamed to `glide.dll.bak` (or an unused hash-suffixed `.bak` if backups already exist). The embedded GameFrame DLL is then installed and the game is launched.
- This includes both older GameFrame experiments and unrecognized external DLLs. Originals and existing backups are never deleted or deliberately overwritten; replacement, original and new SHA-256, and the chosen backup path are recorded in `ScorchedPlanetFix.log`.
- Symlinks, directories, unsafe backup/staging conflicts, or unsuccessful replacements abort safely instead of destroying the existing DLL. Failed activation attempts restore the original where possible.

The game still renders internally at 640×480, but GameFrame shows a centered 4:3 image in a single borderless window fitted to the current desktop. It never switches the physical screen mode or refresh; Glide buffer swaps are capped to 60 FPS.

## Disc edition and integrity rules

Four exact-fingerprint editions are accepted: US 1.08, Europe EN/ES/IT 1.06, Europe FR/DE 1.06, and alternate Europe FR/DE 1.06. Extraction uses the original ISO/CUE-BIN without modifying it and stages `SPLANET/` + required `VOODOO/` assets; obsolete 3dfx drivers and unused software executables are excluded. The complete staged installation (game manifest, patched executable, original `spvoodoo.exe.bak`, INI and bundled GameFrame) is SHA-256/structure checked **before** the `splanet/` directory is published. A failed audit cleans up staging and does not create an unusable installed game folder. Unknown executables are refused. Saves remain writable.

See [supported editions](cd-editions.md) for hashes, language defaults and patch details.

## Validation

The GameFrame integration passed maintainer testing on Wine-staging, GE-Proton and Windows 11: initial installation, second launch, videos, input, Alt+Tab and gameplay. The v2.0.0 release candidate was also tested on Windows 11 and Wine-staging 11.17; Windows CI checks the integrated executable, GameFrame ABI and packaging. The single `ScorchedPlanetFix.log` records hardware inventory, verification, GameFrame diagnostics and game exit information.

The [v1.1.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.1.0) remains available for systems that require nGlide. The development version after v2.0.0 retains the GameFrame-only breaking change: external Glide DLLs are not loaded; the normal launcher backs them up automatically and installs its bundled GameFrame wrapper. The `--verify` command remains read-only and rejects a foreign active Glide DLL.
