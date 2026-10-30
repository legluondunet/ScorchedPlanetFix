# Scorched Planet Setup — 0.1 development preview

This tool installs **from an already copied original CD directory**, not from a mounted ISO or the legacy InstallShield executable.

## Source structure

Expected source root contains `SPLANET` and `VOODOO` directories (names are matched case-insensitively). It may also contain `INSTALL`, `MYSTIQUE`, `VERITE`, etc.

The installer copies all files beneath `SPLANET`, retaining subfolders, then overlays files from `VOODOO`. Both original 3dfx DLLs and game assets remain unmodified. Any `COMPRESS.BIN` is copied but **not extracted** yet; the application displays a warning. This is a provisional mapping of the InstallShield script, not a claim that every CD edition is fully supported.

The configuration file `SPLANET.INI` is created/updated with language and local video path. Supported UI languages are French and German, mapped to the game's numeric language selections 2 and 3. Other game languages already on the disc are preserved in the destination.

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
3. Handle `COMPRESS.BIN` according to the original `CompressSpecials` flags.
4. Add clean uninstallation.
5. Analyze game runtime CD detection and 3dfx/RenderWare launch stability.

## Auditing the original CD before changing the installer

Use **Analyser le contenu du CD…** / **CD-Inhalt analysieren…** on the setup screen.
The button asks where to save `ScorchedPlanet-CD-audit.txt`.
It creates a read-only inventory of the selected source directory:

- Relative file names and byte sizes, without the local absolute CD path.
- Presence of `SPLANET`, `VOODOO`, `SPVOODOO.EXE`, `RWDLVOOD.DLL`, and `COMPRESS.BIN`.
- Contents of textual `SETUP.INI` and `SPLANET.INI` files of at most 128 KiB, to help compare editions and reconstruct legacy setup instructions. Binary or larger files are not embedded.

The report does not extract archives, execute the 1996 setup, inspect the registry, or
change the original CD copy. Only the explicitly chosen report destination is written.
Review the text before sharing it. A full faithful installation, support for English
game data, and the interpretation of `COMPRESS.BIN` remain unverified
until a representative original CD audit can be studied.
