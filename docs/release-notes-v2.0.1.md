# ScorchedPlanetFix v2.0.1 — Menu signature and safer GameFrame installation

This is a maintenance update to the published **v2.0.0** release. It retains the integrated, open-source GameFrame Glide → OpenGL renderer and all previously supported game editions. No original game files or proprietary Glide drivers are distributed.

## What's new in v2.0.1

- **Version/commit signature in the original game menu.** A small red (`#980808`) pixel label, `ScorchedPlanetFix 2.0.1-<commit>`, appears at the bottom right of the presentation surface. The original 4:3 scene and artwork are unchanged. The label remains visible while hovering **Quit** and opening the **Language**, **Quit Game** and **New Game** popup panels, then disappears outside the root menu (including gameplay and video).
- **Automatic Glide DLL replacement.** If the existing `splanet/glide.dll` differs from the bundled verified GameFrame, a normal launch backs up that regular file as **`splanet/glide.dll.bak`**, installs the embedded DLL and verifies its hash before launching. An existing *regular* `glide.dll.bak` is deliberately **replaced**, preventing backup proliferation. The newest displaced DLL remains in the single backup slot; any earlier backup contents are lost. Backups/changes and both SHA-256 hashes are logged.
- **Safer installation from original media.** A freshly extracted and patched game tree is now checked against the complete installed-game manifest (including executable backup, INI and bundled GameFrame) **before** the `splanet/` installation is published. If verification fails, staging is cleaned up rather than presenting a partially installed game.
- **Hardening and cleanup.** Removed obsolete historical Glide fingerprint labels, improved validation and refusal of symlink/non-regular DLL paths, synchronized command-line help/documentation, and expanded automated tests (including 32-bit renderer/menu tests). `--verify` remains strictly read-only.

## Compatibility and usage

- Windows 11 and Linux via Wine/Proton, with functional 32-bit OpenGL drivers for the original 32-bit game.
- Supported original disc profiles are unchanged: US 1.08, Europe EN/ES/IT 1.06, Europe FR/DE 1.06, and the alternate Europe FR/DE ISO 1.06.
- Uses **only the bundled open-source GameFrame Glide DLL**. nGlide/third-party Glide wrappers are not supported, loaded, or bundled.
- The original game remains a centered 4:3 picture in a borderless desktop-sized window, with the existing 60 FPS cap and FadeLow/No-CD fixes.
- Put `ScorchedPlanetFix.exe` next to one supported ISO or CUE/BIN image and launch it with no arguments. On subsequent runs the already-installed `splanet/` directory is verified and reused.
- `--install-only` installs without starting the game; `--verify` validates an installed game without modifying or launching it; `--version` and `--help` report the installed launcher version and options.

## Validation

- Windows CI compiles both x86 GameFrame and x64 launcher, tests the menu-recognition helpers, validates 156 decorated Glide exports and x86 DLL loading, checks the embedded SHA-256 and package/archive manifest.
- The maintainer validated the compact red signature and its behavior on hovering Quit and opening Language, Quit and New Game dialogs under Wine.
- An earlier PR #25 test-merge Windows 11 build successfully installed and relaunched EN/ES/IT 1.06 CUE/BIN media with the complete pre-publication verification. This is **not** a claim that all editions were rerun with the final tagged binary.
- Report problems using `ScorchedPlanetFix.log` beside the executable.

## Known follow-up

- [Issue #26](https://github.com/legluondunet/ScorchedPlanetFix/issues/26): investigate unusually dark terrain and black horizon geometry under Glide. Diagnosis and any rendering changes are deferred; this update does **not** adjust fog, gamma or lighting.

## Licenses

ScorchedPlanetFix: GPL-3.0-or-later. Embedded GameFrame Glide: Apache-2.0. Independent community compatibility project, not affiliated with the original game's publishers or hardware vendors.

Maintained by legluondunet assisted by ChatGPT (OpenAI).
