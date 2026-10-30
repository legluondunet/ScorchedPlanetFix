# ScorchedPlanetFix

**Version 1.0.0 — Windows GUI ISO/CUE-BIN installer and FadeLow patcher for Scorched Planet (1996).**

Rust native **Windows x64 (64-bit) only**, usable on Linux through a compatible 64-bit Wine environment.
The patcher is 64-bit; the original Scorched Planet game remains 32-bit.
No game files, CD ISO, nGlide or proprietary DLLs are bundled.
License: **GNU GPL-3.0-or-later**.

## How to use

Put `ScorchedPlanetFix.exe` beside an existing `splanet`
game directory, or **one** original 3dfx CD image as `.iso` or a
`.cue` file with its referenced `.bin` file(s). For `CUE/BIN`,
keep the `.cue` and referenced files together and select the **CUE**, not
the BIN as a separate image. When an image is detected and `splanet`
is absent, the program
checks its structure, the exact original game executable and
RenderWare DLL hashes, copies the `SPLANET/` game files and the `VOODOO/` 3dfx game components
(with videos) into a staged `splanet` installation. The obsolete CD copies of
`glide.dll`, `sst1init.dll` and `fxmemmap.vxd` are deliberately not installed.
For `CUE/BIN`, the program reads the first supported data track and
normalizes its 2048-byte sectors into a **temporary ISO** for the same
hash-verified installer; no conversion utility, virtual CD mounting or
extra software is required. The temporary ISO is deleted after installation.
It handles `MODE1/2048`, `MODE1/2352`, and CD-ROM XA Form 1
`MODE2/2352`, including a following audio track. Multi-data-track
and unknown formats are rejected rather than guessed. Audio tracks
are **not** installed as CD audio or made available as emulated tracks.
For this approach, free space for the temporary data-track ISO is required.

The patcher creates `splanet.ini` if absent (or updates an existing copy)
to use local videos, enable high-quality video and select a language available
on the detected edition. It preserves existing personal controller, gamma and
other settings. **The original SPVOODOO.EXE is first saved as
SPVOODOO.EXE.bak, then automatically patched before the new splanet folder is
finalized.** An extraction or patch failure leaves no half-installed splanet.
An existing install is never overwritten; the existing original game
executable is automatically backed up and patched when the GUI starts.

The graphical UI displays `1.0.0-<commit>`, the **mandatory automatic
edition-specific patch**, a **Lancer** button, a language selector,
an extraction progress bar and OK result dialogs.
FR/DE automatically receives **FadeLow + détection CD** together;
US and EN/ES/IT receive FadeLow only. There is no opt-out or
manual “Appliquer” action.
A `ScorchedPlanetFix.log` file next to the program provides
Windows/Wine hardware diagnostics and operation details.

FadeLow is protected by edition-specific exact SHA-256 checks. The original
US 3dfx EXE SHA-256 is `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a`
(output: `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45`).
The EN/ES/IT original EXE is `9ab6442553d055a14273ee0e3c5a912986662dc0a3cc8512ac716879b5068e5f`
(output: `dec3977ff83758f3a65bc5fe96a6bafaa064bbf34ef93ee9f35593fb65e173bc`).
The FR/DE original EXE is `eff38cedf2408566060b3c7cf39be59c54d23e67b0a4c7cb0725051aa87e1b72`.
Its owner-tested **combined FadeLow + media-detection** result is
`426882f108348f3e1112597fcbb37af16ef425de5346ecc85425c22c24dc781c`.
It changes four FadeLow bytes at `0x48A6D` and two media-gate bytes at
`0x34CBB`. This edition does not use the FadeLow-only candidate.

**Image verification limitation:** no trusted full-CD per-file reference
manifest is available yet. The tool checks required game files,
structural validity, exact EXE/DLL identities and extracted hashes,
but it cannot prove every data asset is identical to a retail master.
The ISO/CUE-BIN workflow is experimental until tested with the owner's image.

## Supported CD fingerprints

Three owner-supplied edition inventories are documented in
[CD edition fingerprints](docs/cd-editions.md): US (InstallKey 1.08),
EN/ES/IT and FR/DE (InstallKey 1.06). Exact core file hashes are
used to identify the edition. **US and EN/ES/IT use FadeLow alone;
FR/DE uses the owner's gameplay-validated combined FadeLow + CD-detection fix.**
The complete per-file reference inventories are preserved under
`data/cd-inventories/`. No claim of independently verified retail-master authenticity
or complete end-to-end testing of the new GUI installer is made.

## Links

- [Detailed Windows GUI instructions](docs/windows-gui-installer.md)
- [Windows-only build workflow](.github/workflows/rust-fadelow-patcher.yml)
- [PR #5 — Windows GUI (draft)](https://github.com/legluondunet/ScorchedPlanetFix/pull/5)
- [European FadeLow and FR/DE combined-fix analysis](docs/european-fadelow-research.md)
- [Paused high-resolution documentation (PR #4)](https://github.com/legluondunet/ScorchedPlanetFix/pull/4)

The game still needs a separately supplied Glide wrapper such as nGlide;
this project includes the validated FadeLow fix for US and EN/ES/IT,
plus an FR/DE-only media-detection change alongside FadeLow.
No generalized NoCD or high-resolution patch is included.

## Command line (--nogui)

Exactly the same installation and edition-specific patch engine is available
without opening a window. **It works in the current working directory**, not
necessarily the folder containing ScorchedPlanetFix.exe. Only these forms
are supported:

```powershell
.\ScorchedPlanetFix.exe --nogui
.\ScorchedPlanetFix.exe --nogui .\ScorchedPlanet.iso
.\ScorchedPlanetFix.exe --nogui .\ScorchedPlanet.cue
.\ScorchedPlanetFix.exe --nogui .\ScorchedPlanet.cue .\ScorchedPlanet.bin
```

Without arguments after `--nogui`, find exactly one ISO or CUE in the
current directory; CUE automatically locates its referenced BIN file.
Explicit paths can point elsewhere, but the output remains
`./splanet/` in the current working directory. If it already exists
and no image is specified, the CLI only verifies and automatically patches
the existing exact-edition executable; with an explicit image, it refuses
to overwrite `splanet`. The original `SPVOODOO.EXE` is stored in
`splanet/SPVOODOO.EXE.bak` before any patch. The program never starts
the game in `--nogui` mode. For detailed diagnostics, see
`ScorchedPlanetFix.log` in the current directory.

The game's owner confirmed that its CD editions under study **have no
audio CD tracks**. Only the data track is required for extraction;
CUE code also safely ignores any optional later audio tracks but does
not emulate CD audio.

Developed by legluondunet with ChatGPT (OpenAI).
No merging without explicit owner approval.
