# ScorchedPlanetFix v2.0.2 — Windows x64 CLI

This document covers **v2.0.2**, including automatic `glide.dll` migration, strict verification, the root-menu signature, separate logs, and diagnostic commands. The [historical v2.0.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v2.0.0) included GameFrame but required manually moving aside unknown external `glide.dll` files. The [v1.1.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.1.0) remains available for nGlide users.

## Commands

```powershell
.\ScorchedPlanetFix.exe
.\ScorchedPlanetFix.exe --inspect "D:\ISO\Scorched Planet.iso"
.\ScorchedPlanetFix.exe --inspect "D:\ISO\Scorched Planet.cue" "D:\ISO\Scorched Planet.bin"
.\ScorchedPlanetFix.exe --verbose
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --install-only
.\ScorchedPlanetFix.exe --launch-no-verify
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe --version
```

The launcher searches for `splanet/` beside itself. If absent, it discovers one unambiguous supported ISO/CUE image beside the launcher, installs the game, applies validated patches, verifies it and runs it. The original image is never modified. Positional ISO/CUE arguments are no longer accepted. `--inspect image.iso` or `--inspect image.cue [track.bin|track.img]` analyzes unsupported media read-only and writes `ScorchedPlanetFix_inspect.log` without installing anything. `--verbose` expands diagnostics. `--verify` is strictly read-only. `--install-only` installs/migrates without launching. `--launch-no-verify` executes `splanet/spvoodoo.exe` directly without checking files or DLL fingerprints, and never installs, patches or repairs; it is intended only for trusted local game files.

`--launch` and `--3dfx-logo` are unsupported and rejected.

## GameFrame Glide is the only supported renderer

The Windows x64 launcher embeds the Windows x86 Apache-2.0 GameFrame Glide DLL built from pinned source at commit `f195a074a63cc150673c38becdec696163b0b3d1`. For normal verified mode, the embedded DLL's PE32 architecture and SHA-256 are verified before installation and the exact expected SHA-256 is verified again before spawning the 32-bit game. The explicit `--launch-no-verify` switch bypasses these checks and never alters the DLL.

- nGlide and arbitrary external Glide DLLs are **unsupported**.
- No fallback from `SysWOW64`, external launcher directory or third-party paths.
- No `NGLIDE_*` environment variables, configurator settings or 3dfx splash DLL.
- At normal launch, after the immutable game-file audit, any existing **regular** `splanet/glide.dll` whose hash differs from the bundled DLL is renamed to the fixed `glide.dll.bak`, **overwriting an existing regular backup**. The embedded GameFrame DLL is then installed, verified and launched. No new suffixed backups are created.
- This includes older GameFrame experiments and unrecognized external DLLs. A previous `glide.dll.bak` is intentionally replaced by the most recently displaced `glide.dll`; the operation, old and new SHA-256, and whether a backup was replaced are recorded in `ScorchedPlanetFix.log`.
- Symlinks and directories at either the active DLL or backup path, unsafe staging conflicts, or failed replacement attempts cause a safe refusal. If GameFrame activation fails after moving the old DLL to `.bak`, restoration is attempted; an earlier `.bak` replaced during this operation is not recoverable.

The game still renders internally at 640×480, but GameFrame shows a centered 4:3 image in a single borderless window fitted to the current desktop. It never switches the physical screen mode or refresh; Glide buffer swaps are capped to 60 FPS.

## Disc edition and integrity rules

Four exact-fingerprint editions are accepted: US 1.08, Europe EN/ES/IT 1.06, Europe FR/DE 1.06, and alternate Europe FR/DE 1.06. Extraction uses the original ISO/CUE-BIN without modifying it and stages `SPLANET/` + required `VOODOO/` assets; obsolete 3dfx drivers and unused software executables are excluded. The complete staged installation (game manifest, patched executable, original `spvoodoo.exe.bak`, INI and bundled GameFrame) is SHA-256/structure checked **before** the `splanet/` directory is published. A failed audit cleans up staging and does not create an unusable installed game folder. Unknown executables are refused. Saves remain writable.

See [supported editions](cd-editions.md) for hashes, language defaults and patch details.

## Validation

The GameFrame integration previously passed maintainer tests on Wine-staging, GE-Proton and Windows 11: installation, second launch, videos, input, Alt+Tab and gameplay. The v2.0.2 Windows CI checks the integrated executable, exact GameFrame exports and package; an independent final on-device v2.0.2 gameplay check is still recommended. `ScorchedPlanetFix_installer.log` records installation, hardware inventory, verification, DLL provisioning and game exit; `ScorchedPlanetFix.log` contains the GameFrame renderer's Glide/OpenGL diagnostics. The logs are append-only, beside the launcher, and share a `session_id` per launch. `ScorchedPlanetFix_inspect.log` is used only for requested disc-image inspection.

The [v1.1.0 release](https://github.com/legluondunet/ScorchedPlanetFix/releases/tag/v1.1.0) remains available for systems that require nGlide. Version v2.0.2 retains the GameFrame-only breaking change: external Glide DLLs are not loaded; the normal launcher backs up the existing nonmatching regular DLL, replacing the single existing regular backup if necessary, and installs its bundled GameFrame wrapper. The `--verify` command remains read-only and rejects a foreign active Glide DLL.
