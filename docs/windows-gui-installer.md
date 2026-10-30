# ScorchedPlanetFix 1.0.0 — Windows GUI + ISO/CUE-BIN installer

**Status: development build for owner validation; not yet a stable release.**

ScorchedPlanetFix is a Rust **Windows x64-only** graphical application
for modern 64-bit Windows systems. Linux users may run the same x64
executable through a compatible 64-bit Wine environment; there is no
native Linux release or 32-bit patcher build. The original game is
still a 32-bit executable and needs an environment that supports 32-bit games.

## Version and license

The application title is `ScorchedPlanetFix`; immediately below it,
the interface displays `1.0.0-<8-character Git commit>`.
The exact commit is embedded at build time. New code is licensed
GNU GPL-3.0-or-later; see `LICENSE.txt` inside the release ZIP.

No game content, game patches, ISO files, Glide/nGlide binaries
or commercial DLLs are included.

## User flow

1. Place `ScorchedPlanetFix.exe` in a writable folder.
2. In the **same folder**, place **either**
   - a single `.iso` image of the **supported original Windows 3dfx** game, **or**
   - a single `.cue` file with its referenced `.bin` track file(s)
     (keep the file names and relative directories from the CUE intact), **or**
   - an existing directory named `splanet` containing a complete installation.
3. Launch the patcher. The working directory of the executable is detected
   using its own full path, not the shell's current directory.
4. If `splanet` exists, it is **never overwritten**. The patcher uses
   its installed `SPVOODOO.EXE` (or reports an error if missing).
5. If `splanet` does not exist and exactly one image (`.iso` or `.cue`)
   is present, the patcher recognizes its exact 3dfx edition. It extracts
   **only** `SPLANET/` and `VOODOO/` into temporary directories
   `temp/splanet` and `temp/voodoo`, with normalized **lowercase paths**
   and SHA-256 verification. Then it copies the entire `temp/splanet`
   directory into a separate staged game installation, followed by
   `spvoodoo.exe` and `rwdlvood.dll` from `temp/voodoo`.
   Original `glide.dll`, `sst1init.dll` and `fxmemmap.vxd` are
   excluded. The second copy is also SHA-256 verified. After copying,
   the patcher saves `spvoodoo.exe.bak`, applies and verifies the
   edition-specific binary correction, **then** generates `splanet.ini`
   with the portable setting `[scorch]` / `video= video`, the correct
   language and `HighQualityVideo=1`. No absolute video path is stored.
   Only after all these operations succeed is the staging folder
   atomically renamed `splanet`; all temporary extraction directories
   are removed.
6. During extraction a percentage progress bar is displayed. An OK
   dialog reports completion or error. The installer never deletes or
   overwrites the original ISO or a pre-existing `splanet` directory.
7. After extraction, select a language supported by the detected edition in
   **Langue du jeu**. The installer writes the relative `video= video` setting,
   `HighQualityVideo=1` and the selected `language=` code into `splanet.ini`
   without overwriting existing `keymap`, `gamma`, `joystick` or
   personal `settings`. Defaults are English for US and EN/ES/IT, French
   for FR/DE. The original game's language-resource table maps
   English=1, French=2, German=3, Italian=4, Spanish=5.
8. **The correct edition-specific fix is automatically and obligatorily
   applied after extraction**: US and EN/ES/IT receive 4-byte FadeLow;
   FR/DE receives 6-byte FadeLow + CD-media detection. The unmodified
   `spvoodoo.exe` is saved as `spvoodoo.exe.bak` **before** patching.
   Exact source/output SHA-256 hashes and expected PE bytes are checked.
   If extraction or patching fails, the staged installation is discarded,
   so no incomplete `splanet` folder is published.
9. Click **Lancer**. The original game's EXE starts in its installation
   directory. A compatible Glide implementation (e.g. separately
   installed nGlide) must be available; this patcher does not
   bundle or install it.

The interface is responsive during extraction and patching through a
worker thread and messages to the GUI. There is no manual patch checkbox
or `Appliquer` button; `Lancer` is disabled until the mandatory
patch has succeeded. When an existing installation is detected, its
original supported EXE is also backed up and patched automatically.
Unknown EXEs are refused.

## Patch details

The supported patch fixes the **palette fade timer starvation /
Go mission-launch hang** on modern CPUs. The owner has gameplay-validated
**US** and **EN/ES/IT** using the four-byte FadeLow change alone.
The owner also gameplay-validated **FR/DE using FadeLow + a separate
two-byte CD-media gate modification**, after observing "PAS DE CD" with
the four-byte FadeLow-only candidate. The FR/DE combined output is
`426882f108348f3e1112597fcbb37af16ef425de5346ecc85425c22c24dc781c`.
This changes exactly six bytes; it does not create missing CD resources
or guarantee all media dependencies are eliminated.
The new GUI workflow remains untested end-to-end.

| Item | Known verified value |
| --- | --- |
| Supported original SHA-256 | `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a` |
| Output FadeLow SHA-256 | `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45` |
| File offset | `0x489CD` |
| Original bytes | `17 B7 D1 38` |
| Patched bytes | `AC C5 27 37` |
| Backup | `splanet/spvoodoo.exe.bak` for fresh installations (created before modification) |

The independently tested patch binaries work on Linux/Wine/nGlide, but this
new graphical Rust program must still be tested on the owner's actual
disc images before public release. If the program is run again, it
recognizes the already patched executable, verifies its `.bak`
and makes no further change. A verified older backup named
`SPVOODOO.EXE.ScorchedPlanetFix-original.bak` can be migrated safely
to the new `.bak` name without deleting the older copy.

## ISO validation scope — important

The code uses the read-only, pure-Rust `isomage` parser to access
ISO9660/Joliet or UDF filesystem entries without mounting the image.
For CUE/BIN, it first parses the CUE and copies **only the first data
track's 2048-byte user data**, sector by sector, into a temporary ISO
used by that same parser. The temporary ISO is deleted after installation
or on error; the source BIN and CUE remain untouched.

The preflight checks:

- structurally valid filesystem entry extents inside the image;
- for CUE/BIN: safe relative BIN paths with no traversal or symlinks,
  ordered tracks, first-track data format, valid sector headers and
  an ISO9660 `CD001` primary volume descriptor at data sector 16;
- bounded entry count and bounded file sizes;
- safe, unambiguous Windows file and directory names;
- presence of `SPLANET/` and `VOODOO/`;
- presence of `GEOMETRY.RWB`, `SPVOODOO.EXE`, `RWDLVOOD.DLL`
  and Smacker `.SMK` video files; `SPLANET.INI` is optional and
  generated when absent;
- **exact SHA-256** of original `SPVOODOO.EXE` and `RWDLVOOD.DLL`
  against verified retail reference binaries;
- streamed extraction into `temp/splanet` and `temp/voodoo` of
  the game tree plus only two `VOODOO` components, omitting
  `glide.dll`, `sst1init.dll`, `fxmemmap.vxd`; verify exact SHA-256
  between the CD image and each extracted file;
- SHA-256 comparison for every copy from temporary trees into staged
  `splanet`, rejecting name collisions and preserving all game assets;
- canonical lowercase file and directory names across all three editions
  (including US). The source CD image is never renamed or modified.

The installer recognizes three **owner-inventoried 3dfx CD editions** using
exact EXE + GEOMETRY + RWDLVOOD SHA-256 signatures: US (InstallKey 1.08),
Europe EN/ES/IT (InstallKey 1.06), and Europe FR/DE (InstallKey 1.06).
The keys are reported by original `autorun.inf`, not independently proven
executable version resources. See [CD fingerprints](cd-editions.md).
The installer **rejects incompatible or mixed file sets**, including
the distinct Matrox Mystique release or a game EXE of unknown revision.
It does **not** claim to verify every file against an authoritative
full-disc hash manifest: that reference data is not yet available.
Consequently, files that have been altered within an otherwise
well-formed image but retain a valid length cannot always be
recognized as altered from the retail master. A trusted
complete-CD manifest should be added before we claim **full
per-file authenticity**. This is a non-negotiable limitation.

The file layout is based on the user's
[Windows 3dfx Lutris script](https://lutris.net/games/install/39685/view),
NOT the linked [Matrox 86Box script](https://lutris.net/games/install/39693/view).
The original script puts videos under `c:\splanetcd\splanet\video`.
The owner has since confirmed `[scorch]` with `video= video`
is sufficient for the game to find its videos, provided the game starts
with `splanet` as its working directory. The patcher now writes this
portable relative setting and does not embed machine-specific absolute
paths or Windows/Wine drive letters.

## Log file and privacy

`ScorchedPlanetFix.log` is created beside the Windows patcher,
not beside `SPVOODOO.EXE`.
It contains timestamped session starts (UTC UNIX timestamp),
Windows/Wine version and environment, CPU model and core count,
reported memory, active monitor resolutions, D3D9 GPU identification,
connected controllers, image checks, extraction checksum results,
patch activity, failures, success and launch attempts.

The passive Win32 diagnostic routines have been adapted under
GPL-3.0-or-later from
[CrimsonSkiesFix](https://github.com/legluondunet/CrimsonSkiesFix).
They avoid machine-unique identifiers, usernames, serial numbers,
IP addresses, full registry paths and private window titles. The log
can nevertheless contain local game paths and hardware information;
users should review it before posting publicly.

## Building

Requires stable Rust with Windows MSVC target, and no system Python:

```powershell
rustup target add x86_64-pc-windows-msvc
cargo test --target x86_64-pc-windows-msvc --all-targets
cargo build --release --target x86_64-pc-windows-msvc --bin scorched-planet-fix
```

The `.cargo/config.toml` requests static MSVC CRT linkage.
The workflow `.github/workflows/rust-fadelow-patcher.yml` builds
**Windows x86_64 only**, checks the AMD64 PE architecture and GUI subsystem, and packages
the EXE, README, GPL license and SHA256SUMS.

## Workstream boundaries

- High-resolution experiments are **paused**, documented in draft PR #4.
- A **general NoCD patch is not included**: the local video path was
  sufficient for US. Only the owner-tested FR/DE combination includes
  an edition-specific CD-media result change, gated by exact hashes.
  It does not supply missing resources or alter other editions.
- Rust CD installer research in PR #1 remains separate; this new GUI
  includes only the original 3dfx copy layout and strict validation.
- Do not merge this PR or publish a stable release until the owner
  validates an existing `splanet` directory and original 3dfx
  `.iso` and `.cue/.bin` images.

Developed by legluondunet, assisted by ChatGPT (OpenAI).

## Owner's EN/ES/IT validation (2026-10-30)

The owner confirmed that the **EN/ES/IT four-byte FadeLow test executable works in gameplay**. Manual procedure: extract the disc, copy VOODOO game files to SPLANET excluding original `glide.dll`, then create `splanet.ini` with the installed-video path, selected language and `HighQualityVideo=1`. The provided working INI uses `language=4`, independently identified from the game executable's resource pointer table as **Italian**.

This is validation of the manually prepared game and experimental patch, **not yet of the new automatic x64 GUI installer**. The owner's remaining INI values (`keymap`, `gamma`, `joystick`, `settings`, `name`) have not been proven mandatory for a fresh install. Current implementation preserves existing values, supplies only established fallback `settings`/`name` if absent, and does not synthesize custom device bindings.

## CUE/BIN support and limits

`ScorchedPlanetFix.exe` and a single `disc.cue` with its referenced
`disc.bin` file(s) must be accessible together in the working directory.
`FILE` declarations may use relative subdirectories; absolute, parent
traversal and symlink paths are rejected. A bare `.bin` without its
associated `.cue` cannot describe sector sizes and track boundaries
reliably and is **not** auto-detected as a complete image.

Supported first data tracks: `MODE1/2048`, raw `MODE1/2352` and raw
`MODE2/2352` **Form 1 only**. The parser supports later audio
tracks (including multi-BIN track references) by excluding them from
the data conversion, but does **not** reproduce CDDA audio tracks.
Subsequent data tracks, unknown sector modes, malformed or ambiguous
CUE sheets, Form 2 sectors and absent source BIN files fail with a
specific error, without making partial `splanet` installations.

The temporary ISO, stored next to ScorchedPlanetFix in a hidden
`.scorchedplanetfix-cue-*.iso` file, requires enough free space for
the 2048-byte-per-sector data track. Its source CUE/BIN is never
modified or overwritten. Temporary files are removed after the
installer returns normally, whether extraction succeeded or failed.
If the application is forcibly killed mid-conversion, a leftover
hidden temporary ISO may require manual deletion.

**No conversion or mounting step is required from the user for these
supported data tracks.** The owner confirms Scorched Planet has no
CD audio tracks in the studied editions. The installer therefore needs
only their data track. The code does not attempt CDDA emulation.
Do not claim end-to-end testing with the owner's particular CUE/BIN
dumps before those tests are performed.

## Command-line mode (--nogui)

The same Windows x64 binary can be used as a headless installer with
no window opening. The default working directory is the **current
directory**, and all extracted files are written to `./splanet`:

```powershell
.\ScorchedPlanetFix.exe --nogui
.\ScorchedPlanetFix.exe --nogui "C:\images\Scorched Planet.iso"
.\ScorchedPlanetFix.exe --nogui "C:\images\Scorched Planet.cue"
.\ScorchedPlanetFix.exe --nogui "C:\images\Scorched Planet.cue" "C:\images\Scorched Planet.bin"
```

With no image argument, look for exactly one `.iso` or `.cue` in the
current directory (a CUE's BIN is resolved from its FILE entry).
Optional positional `.bin` is allowed only after a CUE and its name
must match that CUE's declared data-track BIN filename. If there
is an existing `splanet` folder and no explicit image, the program
verifies and automatically patches its original EXE; if a particular
image was specified, it refuses to overwrite the existing folder.

The same hash verification, portable relative video setting, language
defaults, lowercase extraction, stage/rollback behavior, mandatory
automatic patch and `spvoodoo.exe.bak` apply to both modes.
CLI does not launch the game and uses no other options. Its log is
`ScorchedPlanetFix.log` in the **current working directory**.
Return status is zero on success and nonzero on failure. The EXE keeps
the Windows GUI PE subsystem to avoid a console appearing in normal
GUI mode; a calling terminal is used for CLI messages if available.
When launching from scripts, explicitly await the process exit to
obtain its exit status (e.g. PowerShell `Start-Process -Wait -PassThru`).

The owner reports that the Scorched Planet CD variants have **no audio
tracks**, so audio-track extraction or CDDA emulation is not necessary
for this game.

## Strict verification and launch

The same Windows x64 executable also supports:

```powershell
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --launch
```

These commands require **no extra arguments** and always use the
`splanet` folder **beside ScorchedPlanetFix.exe**, regardless of the
shell's current directory. This differs intentionally from `--nogui`
which installs into the current directory.

`--verify` is strictly read-only. It checks the exact SHA-256 of the
corrected, recognized executable and its original matching `.bak`;
`splanet.ini` must have `[scorch]` and the portable
`video= video` setting, with a valid selected edition language if
specified. It also verifies each installed **immutable resource** by
name, byte count and SHA-256 from the owner's CD inventory. Mutable
`*.sav` slots are exempt because gameplay legitimately changes them,
and the INI itself is checked semantically rather than comparing its
hash. Other user-added files are allowed. Verification does not
rewrite anything or silently repair a failure.

`--launch` runs the **same full verification before spawning** the
original game executable in the `splanet` working directory; it
refuses to launch a failing installation. It waits for the game to
finish, then reports an abnormal exit code. The graphical **Lancer**
button uses the same verification on a worker thread, so the GUI
remains responsive, and monitors the game's termination.

All expected application errors (missing files, invalid options,
failed verification, installation and patch errors, launching and
process-exit failures) are reported and recorded to
`ScorchedPlanetFix.log`. In the GUI and `--verify`/`--launch`
modes it is located beside the patcher; `--nogui` retains its
current-directory log. If a log cannot be written, the program
attempts a fallback log, but an entirely unwritable system cannot
guarantee persistence.

The verification profiles use the owner-supplied inventories rather
than an independent official retail-master database. The new
GUI/CLI flows remain subject to real Windows and Wine testing.
