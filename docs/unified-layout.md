# ScorchedPlanetFix 2.1 — unified single-game layout

**Development only — PR #36.** This is not an official release and has not
yet passed an end-to-end gameplay regression on native Windows and Wine.

The layout follows CrimsonSkiesFix unified architecture: one launcher, one
fix-owned root INI, one game directory, and separate state and documentation.

```text
ScorchedPlanetFix/
  ScorchedPlanetFix.exe
  ScorchedPlanetFix.ini
  ScorchedPlanetFix.log
  ScorchedPlanetFix_installer.log
  game/
    spvoodoo.exe                 [retail only]
    spvoodoo.exe.bak             [retail original backup]
    glide.dll                    [retail GameFrame only]
       -- or --
    splanetw.exe                 [Matrox only]
    splanetw.exe.bak             [Matrox original backup]
    msi95.dll                    [Matrox clean-room driver only]
    msi95.dll.bak                [Matrox original proprietary driver backup]
    splanet.ini                  [the game's original native INI]
    ...                          [game resources, video, saves]
  state/
    ScorchedPlanetFix.install-receipt.txt
  docs/
    README.md
    LICENSE
    ScorchedPlanetFix.ini.example
    build-info.txt
```

**Exactly one edition is installed at a time.** The launcher detects the
edition by the sole supported game executable inside `game/`. Both active
executables present, missing executables, damaged files and unsafe symlinks
are refused rather than guessed. The original executable and driver
fingerprints are verified by exact SHA-256; no original copyrighted files
are distributed in the launcher.

Both renderers are embedded in ScorchedPlanetFix.exe. The active renderer
DLL must be next to the game's executable for native Windows/Wine DLL
resolution. No extra `third_party/` or `dlls/` directory is required.

## Settings

`ScorchedPlanetFix.ini` lives next to the launcher, not in `game/`:

```ini
[Graphics]
FPSLimit=60

[matrox]
Dither=0

[Cheats]
UnlimitedFuel=0
```

- FPSLimit under `[Graphics]` is shared by retail and Matrox: 0 means no frame pacing,
  1..240 specifies an upper bound (60 by default).
- Dither=0/1 affects only Matrox and is experimental.
- Native game preferences stay in `game/splanet.ini` and are not mixed
  with these fix-specific options.
- Existing custom root INI values and unrelated sections are preserved;
  malformed/duplicate known keys are rejected.
- Settings are applied on launch and do not require separate CLI flags.

## Safety and original Matrox media

- A fresh Matrox install is extracted into a private staging folder; the
  complete original 66-file OEM manifest is verified before processing.
- Inside that PRIVATE stage the original splanetw.exe and original MSI95
  are preserved under fixed `.bak` names; only the patched executable
  and clean-room MSI95 are put at their normal active names.
- A second complete 315 MB Matrox runtime copy is NOT installed.
- The combined game/ tree is validated before atomic publication.
- Existing user saves/configs and arbitrary unknown files are never
  overwritten during automatic installation.
- The ordinary launcher can update a previously recognized open-source
  MSI95 DLL with rollback. Unknown MSI95 DLLs are never replaced.
- For retail, original patched-EXE backup and verified bundled GameFrame
  replacement rules remain unchanged.

## Old layouts

The new launcher **does not automatically migrate** the old
`splanet/`, `scorchedplanet-matrox/` or
`scorchedplanet-matrox-runtime/` directories. If any appear beside
the launcher, the operation refuses to proceed and preserves all files.
For testing, use a new empty working directory and the original ISO/CUE
image. Back up saved games before any manual relocation.

## Command behavior

- `ScorchedPlanetFix.exe`: use installed game or auto-identify a
  supported CD, install and launch.
- `--install-only`: install or prepare a verified installed edition
  without launching.
- `--verify`: read-only game+renderer verification; no repairs, INI
  creation, docs generation or installation.
- `--launch-no-verify`: direct unverified launch, no game-file changes.
- `--inspect image.iso` or `--inspect image.cue [track.bin|track.img]`:
  inspect a disc image without installation.

Installer and runtime logs are reset per launch and stored beside the EXE.
`state/` retains the informational installation receipt. `docs/`
contains README, license, INI example and build metadata, without
overwriting existing user-edited reference files.

## Validation checklist

1. From an empty folder, install retail 3dfx, verify game/ files,
   read the two logs and test menu, movies, controls, FPS limit and exit.
2. From a separate empty folder, install Matrox CUE/IMG, verify the
   66-file manifest plus its `.bak` originals and embedded MSI95.
3. Test `FPSLimit=30`, `FPSLimit=60`, `FPSLimit=0` on both editions;
   test `[matrox] Dither=0` vs `Dither=1` only for Matrox.
4. Repeat on Windows 11 and Wine/Proton, including Alt-Tab, full
   missions, video, saves, and restoration after game exit.
5. Confirm `--verify` makes no game/INI/docs changes and refuses
   unsupported files; ensure previous-layout directories remain intact.
6. Do not merge PR #36 until CI **and** the requested on-device game
   tests have passed.

## 2026-11-19 Matrox quality and BMP cleanup

Fresh Matrox installs now write native `game/splanet.ini` with English language
`language= 1`, quality video `HighQualityVideo= 1`, `gamma= 1.300000`
and `settings= 0 2 1 1 5 95 127 0 0 5`, directly based on the
player-provided INI tested with high graphics and shadows enabled.
This native INI is independent from the fix-owned launcher INI.

An older exact auto-generated Matrox native INI is upgraded to these values
only when byte-for-byte identical to that known installer template.
Custom/native player INIs and saved games are not overwritten.

The clean-room MSI95 now disables **automatic** diagnostic BMP files
(frame-number snapshots, blue-sprite diagnostics and automatic ownership
coverage). F12 manual capture is still available by explicit request.
The release-mode filename and directory are unchanged for manual captures.

`[Graphics] FPSLimit=60` replaces the old `[scorch] FPSLimit`.
The settings reader accepts old configurations during read-only verification;
on a normal verified launch the migrator moves the legacy FPS value into the
new section without changing the player's numeric limit. New configurations
also include `[Cheats] UnlimitedFuel=0`, **disabled by default**.

**Fuel cheat status:** no verified native unlimited-fuel code or trusted
patch point has been established for this exact OEM executable.
`UnlimitedFuel=1` produces an explicit unsupported-feature error rather
than silently pretending the fuel is unlimited. This first INI entry is
a reserved opt-in configuration until a version-specific patch is tested
in-game. The known `FATAL` cheat grants weapons and ammunition and
must not be falsely represented as infinite flight fuel.
