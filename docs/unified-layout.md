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
ALIAH=0
FATAL=0

[Debug]
Log=0
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

## 2026-11-19 — Native keyboard cheat codes

New default `[Cheats]` keys in the launcher-root INI:

- `ALIAH=0`: **invulnerability** (source spelling ALIAH; some code lists
  instead say `ALLAH`). The in-game keyboard code is documented, but
  automatic injection is not tested or implemented for the current two
  renderers. The entry remains disabled by default.
- `FATAL=0`: **all weapons and ammunition**. Native in-game code;
  automated injection not yet verified. This is not infinite flight fuel.
- `UnlimitedFuel=0`: still awaiting an authenticated native executable
  fuel-consumption patch.

Do not represent a `=1` INI toggle as a working cheat at this stage. The
launcher rejects an enabled unimplemented code, explaining that users may
**type the native code during gameplay**. Unknown entries, comments and
existing player values are preserved by the INI migrator. The settings
parser accepts only 0 and 1, rejects duplicate assignments and preserves
read-only verification.

## [Debug] runtime log control (2026-11-20)

The generated `ScorchedPlanetFix.ini` is now the SAME fully commented,
user-facing sample as `docs/ScorchedPlanetFix.ini.example`. Every option
has a description, including unavailable automatic cheat switches.

- `[Debug] Log=0` is the default: the bundled MSI95 and the Scorched-specific
  GameFrame shim do not write their runtime diagnostics to a file or stderr.
  The historical GameFrame shim fallback to stderr is explicitly disabled in
  the project's pinned, temporary build patch.
- `Log=1` enables `ScorchedPlanetFix.log` beside the launcher; this log
  is truncated before every new game session, never cumulative.
- The separate `ScorchedPlanetFix_installer.log` is retained and remains
  noncumulative regardless of `Log`, so installation/verification failures
  can always be diagnosed. `--verify` does not generate a runtime log.
- Existing root INI values, comments, and unknown sections are preserved;
  a missing `[Debug] Log=0` option is inserted with its description, without
  overriding a user's `Log=1` choice.

### Why the Matrox executable .bak exists

The original `game/splanetw.exe.bak` is the exact 840704-byte Matrox OEM
binary preserved from the user's disc before the two checked compatibility
patches are applied to `game/splanetw.exe`. It is NOT a launcher fallback.
The currently implemented 66-file OEM verification authenticates this exact
original backup by SHA-256, and separately verifies the patched active EXE;
removing it without redesigning that audit would make `--verify` fail.
The original proprietary `game/msi95.dll.bak` is preserved for the same
manifest audit and is never loaded at runtime. No automatic deletion.

## One-time first-play logs (2026-11-20)

On a **fresh first installation**, the installer writes
`ScorchedPlanetFix_installer.log` alongside the launcher. The INI **always
starts with `[Debug] Log=0`**, but the first verified game execution
automatically forces one complete `ScorchedPlanetFix.log` from either the
GameFrame or MSI95 renderer, without changing `Log=0` on disk.

- Successful first game exit (including the original game's known exit code
  1): write `state/ScorchedPlanetFix.first-launch-logged.txt` as the
  one-time completion marker. This marker is separate from the informational
  installation receipt. The game resources and player's saves remain
  untouched.
- Subsequent verified launches with `Log=0`: leave the first game's runtime
  log **untouched** (retain diagnostics) and do not generate additional
  runtime output; the launcher still creates a new noncumulative installer
  log on each invocation for diagnostics.
- Subsequent `Log=1` launches: force runtime logging and truncate the
  runtime log for that session, regardless of the marker.
- `--install-only` never consumes the first-play opportunity. Spawn failure,
  interruption or abnormal game exit also leave the marker absent so
  diagnostics are collected again next time.
- `--verify` and `--launch-no-verify` keep their established semantics:
  no first-play marker creation or forced first-play diagnostics.
- No one-time `Log=1` rewrite of the INI is needed. The launcher passes
  an **effective temporary log flag to the child renderer**, and only the
  state receipt records completion.

## Repeatable Matrox F12 image and diagnostic-map pairs (2026-11-20)

Each **distinct** keypress generates a new, non-overwriting paired set
under `game/`:
`matrox_capture_0001_frame.bmp` plus
`matrox_capture_0001_coverage.bmp`, increasing serial numbers across
restarts. The driver skips a serial if **either** file exists and
opens new files with `CREATE_NEW` as a final guard against overwrites.
It ignores F12 autorepeat until the key is released. Automatic BMP
capture and the default `[Debug] Log=0` are unchanged.
