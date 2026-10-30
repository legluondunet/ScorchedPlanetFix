# ScorchedPlanetFix

**Version 1.0.0 — Windows GUI ISO installer and FadeLow patcher for Scorched Planet (1996).**

Rust native **Windows only**, usable on Linux through Wine.
No game files, CD ISO, nGlide or proprietary DLLs are bundled.
License: **GNU GPL-3.0-or-later**.

## How to use

Put `ScorchedPlanetFix.exe` beside an existing `splanet`
game directory, or a single supported original Windows 3dfx CD ISO.
When an ISO is detected and `splanet` is absent, the program
checks its structure, the exact original game executable and
RenderWare DLL hashes, copies the `SPLANET/` and `VOODOO/`
trees (with videos) into a staged `splanet` installation and
updates `splanet.ini` to use local videos. It never overwrites
an existing game directory.

The graphical UI displays `1.0.0-<commit>`, a checkbox for
**Correction du blocage du fondu de palette (FadeLow)** with
an explanatory tooltip, **Appliquer** and **Lancer** buttons,
a percentage extraction bar and OK result dialogs.
A `ScorchedPlanetFix.log` file next to the program provides
Windows/Wine hardware diagnostics and operation details.

The original 3dfx game exe must match SHA-256
`fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a`.
The output FadeLow SHA-256 is
`0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45`.

**ISO verification limitation:** no trusted full-CD per-file reference
manifest is available yet. The tool checks required game files,
structural validity, exact EXE/DLL identities and extracted hashes,
but it cannot prove every data asset is identical to a retail master.
The ISO workflow is experimental until tested with the owner's image.

## Links

- [Detailed Windows GUI instructions](docs/windows-gui-installer.md)
- [Windows-only build workflow](.github/workflows/rust-fadelow-patcher.yml)
- [PR #5 — Windows GUI (draft)](https://github.com/legluondunet/ScorchedPlanetFix/pull/5)
- [Paused high-resolution documentation (PR #4)](https://github.com/legluondunet/ScorchedPlanetFix/pull/4)

The game still needs a separately supplied Glide wrapper such as nGlide;
this project includes only the validated FadeLow timing change, no NoCD
or high-resolution patch. Developed by legluondunet with ChatGPT (OpenAI).
No merging without explicit owner approval.
