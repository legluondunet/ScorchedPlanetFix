# ScorchedPlanetFix 2.0.0 — Windows x64 CLI

This document covers the **v2.0.0 candidate**. The published `v1.1.0` release is unchanged.

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
- Unknown existing `splanet/glide.dll` files are **preserved and rejected**. The launcher instructs the user to move their DLL aside manually; it never silently replaces it.
- Only known older GameFrame experiment fingerprints can be migrated automatically, with their original bytes preserved in a unique backup.

The game still renders internally at 640×480, but GameFrame shows a centered 4:3 image in a single borderless window fitted to the current desktop. It never switches the physical screen mode or refresh; Glide buffer swaps are capped to 60 FPS.

## Disc edition and integrity rules

Four exact-fingerprint editions are accepted: US 1.08, Europe EN/ES/IT 1.06, Europe FR/DE 1.06, and alternate Europe FR/DE 1.06. Extraction uses the original ISO/CUE-BIN without modifying it and stages `SPLANET/` + required `VOODOO/` assets; obsolete 3dfx drivers and unused software executables are excluded. Patch and original `spvoodoo.exe.bak` are SHA-256 checked. Unknown executables are refused. Saves remain writable.

See [supported editions](cd-editions.md) for hashes, language defaults and patch details.

## Validation

The original GameFrame integration passed maintainer testing on Wine-staging, GE-Proton and Windows 11: initial install, second launch, videos, input, Alt+Tab and gameplay. The v2-only code removal and packaging are pending their own CI validation. The single `ScorchedPlanetFix.log` records hardware inventory, verification, GameFrame's diagnostics and game exit information.

The [published v1.1.0](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.1.0) remains available as the historical release supporting nGlide. No v2 tag or public release has been created.
