# Scorched Planet Setup — 0.1 development preview

This tool installs **from an already copied original CD directory**, not from a mounted ISO or the legacy InstallShield executable.

## Source structure

Expected source root contains `SPLANET` and `VOODOO` directories (names are matched case-insensitively). It may also contain `INSTALL`, `MYSTIQUE`, `VERITE`, etc.

The installer copies all files beneath `SPLANET`, retaining subfolders, then overlays files from `VOODOO`. Both original 3dfx DLLs and game assets remain unmodified. If a `COMPRESS.BIN` happens to be present, it is copied unchanged and a warning is displayed; **the file is not required** and its presence on the owner's French/German CD has not been confirmed. This is a provisional copy strategy, not a verified reconstruction of the InstallShield script.

The configuration file `SPLANET.INI` is created/updated with language and local video path. The game-language picker currently offers English, French and German (provisional SPLANET.INI numeric codes 1, 2 and 3). When a supported original SETUP.INI is found, languages disabled for that CD are unavailable. The setup UI currently uses the French fallback text for the English game-language choice; full UI localization is a separate task. Other game languages already on the disc are preserved in the destination. The numeric game-language mapping still needs testing with actual installations.

## Artwork

The original installer's `DIALOG.BMP`, `BANNER.BMP`, `COMPANY.BMP`, and `CPYRIGHT.BMP` were referenced by InstallShield 3's script, often via `SUPPORTDIR`. The new setup searches `INSTALL` for those files and displays them if accessible. They might instead be bundled inside old InstallShield support archives; extracting those is a **separate task**. A readable fallback UI remains functional without the artwork. Do not commit commercial artwork.

## Safety, limitations

- Windows 11 / Windows x64 graphical installer.
- No requirement for a mounted CD **at install time**. Removing a separate game-runtime CD check is a future compatibility task.
- No modifications of the original executable or DLLs.
- No Glide translation or 3dfx crash fix is included in this initial PR.
- No DirectX 1996 installer is invoked.
- An existing target folder is never overwritten. File transfer happens to a sibling staging folder; a failed transfer deletes only that newly created staging directory.
- The full game and videos can occupy considerable disk space.
- The copy plan is generic pending verification against the exact original media; the build must be tested against the owner's CD copy.

## Build

```powershell
cargo build --release
# Executable: target\release\scorched-planet-setup.exe
```

A Windows GitHub Actions workflow builds and runs unit tests using synthetic sample data. It does not redistribute the commercial CD.

## Next milestones

1. Verify exact source mapping using the original CD and InstallShield `SETUP.INI`.
2. Extract original BMPs from the InstallShield support data if they are not separate files.
3. Use a real CD audit to identify any actual packed resources, then implement only the extraction operations justified by the original media. Do not assume a `COMPRESS.BIN` exists.
4. Add clean uninstallation.
5. Analyze game runtime CD detection and 3dfx/RenderWare launch stability.

## Auditing the original CD before changing the installer

Use **Analyser le contenu du CD…** / **CD-Inhalt analysieren…** on the setup screen.
The button asks where to save `ScorchedPlanet-CD-audit.txt`.
It creates a read-only inventory of the selected source directory:

- Relative file names and byte sizes, without the local absolute CD path.
- Presence of `SPLANET`, `VOODOO`, `SPVOODOO.EXE` and `RWDLVOOD.DLL`; also reports an optional `COMPRESS.BIN` if one exists.
- Contents of textual `SETUP.INI` and `SPLANET.INI` files of at most 128 KiB, to help compare editions and reconstruct legacy setup instructions. Binary or larger files are not embedded.

The report does not extract archives, execute the 1996 setup, inspect the registry, or
change the original CD copy. Only the explicitly chosen report destination is written.
Review the text before sharing it. A full faithful installation, support for English
game data, and the need for any original InstallShield extraction step remain unverified
until a representative original CD audit can be studied.

## Findings from a user-supplied original SETUP.INI (edition 1.06)

The original configuration includes these values under `[Criterion Studios]`:

| Key | Value | Interpretation |
| --- | --- | --- |
| `English` | `1` | Offered by this CD's original setup |
| `Francais` | `0` | Not offered by this edition |
| `Deutsch` | `0` | Not offered by this edition |
| `Italiano` | `1` | Offered by the original setup; not yet selectable in our Rust UI |
| `Espagnol` | `1` | Offered by the original setup; not yet selectable in our Rust UI |
| `DefaultLanguage` | `1` | Original setup's default language selection; this is not proof of the game's `SPLANET.INI` numeric encoding |

The setup also defines `[SPlanet]` as a common DOS/Windows installation, and
`[Voodoo]` as a Windows-only implementation with
`WinAppFile=SPVOODOO.EXE`, `SelectWin=1`, `SelectDOS=0`,
`DirectX=1`, `DLLs=1`, `VXDs=1`, `MaxDetail=1`,
`SpecialBinaries=0`, `CopyVideos=0`, `SizeData=63`, and `SizeWin=1`.
These keys are original install-configuration flags and do not by themselves
identify the source filenames to be copied or justify installing legacy drivers
on a modern Windows system.

`CompressSpecials=1` occurs only in the `[Mystique]` section, alongside
`SpecialBinaries=1`. The supplied `SETUP.INI` never names `COMPRESS.BIN`,
so that file must **not** be considered a prerequisite for the Voodoo variant.

The Rust parser reads `SETUP.INI` from the disc root,
`INSTALL/SETUP.INI`, or `INSTALL/WINDOWS/SETUP.INI` when present.
If it finds a well-formed language manifest, it blocks languages explicitly
disabled on that CD. Missing manifests remain possible for backward-compatible
preview builds, but their language availability cannot be verified.
English (`language=1`) is a provisional candidate requiring a real-game
installation test, not a reverse-engineered guarantee. Italian and Spanish
remain future setup options.
