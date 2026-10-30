# ScorchedPlanetFix

**Version 1.0.0 — Windows GUI ISO installer and FadeLow patcher for Scorched Planet (1996).**

Rust native **Windows x64 (64-bit) only**, usable on Linux through a compatible 64-bit Wine environment.
The patcher is 64-bit; the original Scorched Planet game remains 32-bit.
No game files, CD ISO, nGlide or proprietary DLLs are bundled.
License: **GNU GPL-3.0-or-later**.

## How to use

Put `ScorchedPlanetFix.exe` beside an existing `splanet`
game directory, or a single supported original Windows 3dfx CD ISO.
When an ISO is detected and `splanet` is absent, the program
checks its structure, the exact original game executable and
RenderWare DLL hashes, copies the `SPLANET/` game files and the `VOODOO/` 3dfx game components
(with videos) into a staged `splanet` installation. The obsolete CD copies of
`glide.dll`, `sst1init.dll` and `fxmemmap.vxd` are deliberately not installed.
The patcher creates `splanet.ini` if absent (or updates an existing copy)
to use local videos. It never overwrites an existing game directory.

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

## Supported CD fingerprints

Three owner-supplied edition inventories are documented in
[CD edition fingerprints](docs/cd-editions.md): US (InstallKey 1.08),
EN/ES/IT and FR/DE (InstallKey 1.06). Exact core file hashes are
used to identify the edition. **FadeLow is still supported only for
the verified US executable**, not the two European binaries.
The complete per-file reference inventories are preserved under
`data/cd-inventories/`. No claim of independent retail-master authenticity
or of successful European gameplay testing is made.

## Links

- [Detailed Windows GUI instructions](docs/windows-gui-installer.md)
- [Windows-only build workflow](.github/workflows/rust-fadelow-patcher.yml)
- [PR #5 — Windows GUI (draft)](https://github.com/legluondunet/ScorchedPlanetFix/pull/5)
- [European FadeLow static analysis (experimental, untested)](docs/european-fadelow-research.md)
- [Paused high-resolution documentation (PR #4)](https://github.com/legluondunet/ScorchedPlanetFix/pull/4)

The game still needs a separately supplied Glide wrapper such as nGlide;
this project includes only the validated FadeLow timing change, no NoCD
or high-resolution patch. Developed by legluondunet with ChatGPT (OpenAI).
No merging without explicit owner approval.
