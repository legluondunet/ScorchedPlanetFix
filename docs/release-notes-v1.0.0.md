# ScorchedPlanetFix v1.0.0 — release notes (draft)

> **Draft only.** These notes are prepared for a future GitHub Release; no tag, release or published asset exists yet. Do not publish them until the release ZIP has been built and checked from the intended tag.

## Scorched Planet (1996), ready for modern systems

ScorchedPlanetFix is a GPL-3.0-or-later Windows x64 console utility for installing, patching, verifying and launching your own copy of *Scorched Planet*. It also runs under Wine on Linux.

### Highlights

- **Three supported CD editions:** US (InstallKey 1.08), Europe EN/ES/IT (1.06) and Europe FR/DE (1.06).
- **Edition-specific FadeLow + No-CD fixes:** six modified bytes per supported original executable, with strict SHA-256 verification.
- **ISO and CUE/BIN installation:** extracts the game's files without requiring the original CD to remain mounted for the gameplay scenarios tested.
- **Integrity and safety:** preserves the original executable as `spvoodoo.exe.bak`, verifies immutable game assets, rejects unknown binaries and preserves user save files.
- **nGlide support:** copies a user-provided 32-bit `glide.dll` and configures the game without changing global nGlide settings.
- **English-only, console-only interface:** automatic installation, `--verify`, `--launch`, `--help` and `--version`; no obsolete `--nogui` option.
- **No bundled game content or proprietary Glide binaries.**

### Compatibility tested

The project owner validated **CUE/BIN extraction, verification and no-CD gameplay** on **Wine 11.17** for all three editions.

On **native Windows 11**, the owner additionally reported successful `--version`, verification, extraction, No-CD and gameplay tests. The submitted logs confirm **EN/ES/IT 1.06 CUE/BIN** extraction, FadeLow + No-CD patching, original backup and a successful `--verify` (162 checked files), plus **US 1.08 ISO** extraction, patching and verified original backup. They do not independently record a US `--verify` session or the game's runtime; successful gameplay is owner-reported. Native Windows validation of the **FR/DE** edition is not yet documented.

The game and installer have **not** been exhaustively tested on every mission, Windows-native setup, GPU or driver combination. This is a statement of the tests performed, not a universal compatibility guarantee.

### Installation

1. Download the Windows x64 ZIP from the eventual release assets. Extract it into a directory.
2. Put your original ISO image or CUE/BIN image in that directory.
3. Install [nGlide](https://www.zeus-software.com/downloads/nglide) separately or provide your own compatible 32-bit `glide.dll` next to `ScorchedPlanetFix.exe`.
4. Open a console in that directory and run:

   ```powershell
   .\ScorchedPlanetFix.exe
   .\ScorchedPlanetFix.exe --verify
   .\ScorchedPlanetFix.exe --launch
   ```

Read the [README](../README.md) for explicit image paths, Wine instructions and troubleshooting.

### Expected release assets

- `ScorchedPlanetFix-v1.0.0-Windows-x64.zip`
- ZIP includes `ScorchedPlanetFix.exe`, `LICENSE.txt`, `README.md` and `SHA256SUMS.txt`.

The exact SHA-256 of the published ZIP and executable **must be generated from the final release build**. Do not reuse a digest from a prior development artifact.

### Licensing

ScorchedPlanetFix is licensed **GPL-3.0-or-later**. You must provide your own original *Scorched Planet* media and nGlide. No copyrighted game files, disc images, proprietary 3dfx components or nGlide DLLs are included.

**Credits:** legluondunet, assisted by ChatGPT (OpenAI). Independent community project, unaffiliated with the game's and nGlide's rights holders.

---

*Release checklist:* confirm clean CI on the intended tag, build artifacts, ZIP checksum, Windows PE console subsystem, game smoke tests, links and license text; then create the GitHub Release.