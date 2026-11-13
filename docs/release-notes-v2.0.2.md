# ScorchedPlanetFix v2.0.2 — Independent logs and streamlined CLI

This maintenance release builds on v2.0.1. It retains GameFrame Glide → OpenGL and all previously supported retail editions without bundling any copyrighted game content.

## What's new

- **Separate installer and renderer logs.** `ScorchedPlanetFix_installer.log` contains extraction, installation, verification, hardware inventory, launch and process errors. `ScorchedPlanetFix.log` is the DLL's Glide/OpenGL rendering log. Both live beside the launcher, append rather than overwrite previous sessions, and carry a shared `session_id` for troubleshooting.
- **Direct launch opt-out:** `ScorchedPlanetFix.exe --launch-no-verify` starts the installed `splanet/spvoodoo.exe` without file existence checks, SHA-256 validation or GameFrame DLL verification, and without any installer, patcher or repair operations. The operating system reports missing executable errors. Use it only with trusted files; normal no-argument launches and `--verify` preserve strict validation.
- **Read-only unsupported-disc inspection:** `ScorchedPlanetFix.exe --inspect image.iso` or `--inspect image.cue [track.bin|track.img]` writes `ScorchedPlanetFix_inspect.log` without installing or launching. A separately specified BIN/IMG filename may differ from the CUE declaration for inspection only; disc parsing and filesystem bounds remain checked.
- **`--verbose`** for additional diagnostics, combinable with the supported options; `--help`, `-h`, `--version`, `--verify`, and `--install-only` remain available.
- **Simpler invocation:** the default `ScorchedPlanetFix.exe` still autodetects a supported image beside itself and installs/launches if needed; `--install-only` installs without launching. Positional ISO/CUE arguments on normal launch and `--install-only` have been removed.
- **Repository cleanup:** removed unused installer/Glide diagnostic wrappers and one-shot historical publishing workflows. Updated CI GitHub Actions to maintained Node 24 variants without changing runtime compatibility.

## Compatibility and preserved behavior

- Windows 11 and Linux via Wine / Proton, requiring functional 32-bit OpenGL drivers.
- Supported original disc profiles unchanged: US 1.08, Europe EN/ES/IT 1.06, Europe FR/DE 1.06, and alternate Europe FR/DE 1.06 image.
- Keeps the centered 4:3 image in a borderless fullscreen desktop window, 60 FPS cap, version/commit signature in the original root menu, validated FadeLow and No-CD patches, and automatic migration of a nonmatching regular `glide.dll` to a single `glide.dll.bak` before provisioning bundled GameFrame on **normal launches**.
- `--verify` remains read-only, preserves user saves, and rejects unknown or damaged files.

## Validation

- GitHub Windows x64 CI builds/tests the launcher and pinned Windows x86 GameFrame DLL, audits 156 decorated exports, and checks the resulting distribution archive.
- DLL startup smoke checks creation of the renderer log and session marker. No-media and `--launch-no-verify` tests verify no accidental installation or file changes.
- Real-game testing of this exact v2.0.2 binary on each Wine/Proton/Windows combination is not implied by CI; please report any regressions together with both log files.

## Known follow-up

- [Issue #26](https://github.com/legluondunet/ScorchedPlanetFix/issues/26): investigate unusually dark terrain and black horizon geometry. This release does not modify fog, gamma, lighting or transparency rendering.

## Licenses

ScorchedPlanetFix: GPL-3.0-or-later. Embedded open-source GameFrame Glide: Apache-2.0. Independent community compatibility project, unaffiliated with the game's original publishers.

Maintained by legluondunet assisted by ChatGPT (OpenAI).
