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
The original proprietary MSI95 is authenticated during ISO installation
and then discarded. The installed manifest deliberately omits this replaced
DLL; an exact historical `game/msi95.dll.bak` is deleted on upgrade.

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

## Upgrade of the installer-owned Matrox driver (2026-11-20)

Old copies of `game/msi95.dll` from our prior experimental builds must
NOT cause `Modified or unsupported Matrox runtime` after upgrading only
ScorchedPlanetFix.exe. The installer now authenticates the 66 OEM files
and both the preserved original EXE/driver backups plus active patched EXE,
then replaces the **installer-owned** active DLL automatically if its hash
differs or the DLL is missing. A temporary rollback file is deleted
after success; no permanent backup of the provided driver is kept.
No installed proprietary DLL backup is required. A historical
`game/msi95.dll.bak` is removed only when its SHA-256 matches the exact
authenticated original OEM DLL; other files are preserved.

The operation also works in `--install-only`, but not in read-only
`--verify`. Custom savegames and the native `game/splanet.ini` are
never modified by this DLL update.

## GameFrame Glide update backups (2026-11-20)

A provided `game/glide.dll` is managed just like Matrox's active
`game/msi95.dll`: a differing regular DLL is automatically replaced with
the embedded binary; an identical DLL is not rewritten; an absent DLL is
installed. During a replacement a `.glide-gameframe-rollback.tmp` copy of
the previous file is held in `game/` until the new DLL is hash-verified,
then it is removed. It is restored on verification failure. No permanent
`glide.dll.bak` is created or modified. An existing old-style backup is
considered user data, so the installer leaves it untouched.

Nonregular DLLs, symlinks and occupied transaction paths are refused.
`--verify` remains read-only and `--launch-no-verify` never modifies
renderer components. This policy is common to retail and Matrox
installer-managed DLLs; an obsolete `msi95.dll.bak` from an earlier
Matrox installation is safely cleaned up if its OEM hash is exact.

## Matrox game NULL guard installation and upgrade (2026-11-20)

The installer-managed `game/splanetw.exe` now receives a narrowly
scoped, signature-verified 16-byte guard for the previously crashing
NULL resource at `0x00494965`, dispatching it to the game's own
NULL-return handler at `0x004949E9`. The original Matrox executable
is always preserved at `game/splanetw.exe.bak` as the immutable OEM
reference, *not* as a newly written backup. Existing versions that
have the exact old installer-patched EXE SHA-256 are upgraded
automatically via temporary staging and rollback; other modified
executables are rejected. New installations get the guarded
binary directly from the user's ISO, never from bundled
copyrighted game assets.

`--verify` remains read-only and strictly checks the current
guarded EXE; `--install-only` is allowed to upgrade.
The patch must be validated by user gameplay after the confirmed
access-violation fix, and PR #36 is still experimental.

## Matrox original DLL is never backed up (2026-11-20)

The installer retains `game/splanetw.exe.bak` to authenticate the original
OEM executable, but no longer keeps any DLL backup. The original proprietary
`msi95.dll` remains one of the **66 files validated on the user's ISO**;
for an installed game it is replaced by the embedded clean-room DLL and
therefore excluded from the **65 remaining immutable CD entries**.
An old `game/msi95.dll.bak` from a previous installation is automatically
deleted only if its checksum exactly matches the OEM CD manifest; unknown
or user-customized backups are left untouched. Same noncumulative logs,
no change to saves or native INI.

The OEM Matrox EXE additionally receives an experimental 16-byte guard for
misaligned briefing object pointers (observed `EAX=2`, fault at
`0x004858CE`) on repeated briefing clicks. Exact historical install-patched
EXEs are upgraded transactionally; the protected original `.exe.bak`
is unchanged. This patch remains pending in-game confirmation.
