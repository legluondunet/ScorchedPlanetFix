# ScorchedPlanetFix 1.0.0 — Windows GUI + ISO installer

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
   - a single ISO image of the **supported original Windows 3dfx** game, **or**
   - an existing directory named `splanet` containing a complete installation.
3. Launch the patcher. The working directory of the executable is detected
   using its own full path, not the shell's current directory.
4. If `splanet` exists, it is **never overwritten**. The patcher uses
   its installed `SPVOODOO.EXE` (or reports an error if missing).
5. If `splanet` does not exist, and exactly one ISO is present, the
   patcher parses it, validates the supported 3dfx edition, merges the
   original `SPLANET/` then `VOODOO/` trees into a temporary directory,
   checks all installed file transfers, generates `splanet.ini` if absent
   (or preserves its settings if present), writes the local `video=`
   path and finally atomically names the new
   installation `splanet`.
6. During extraction a percentage progress bar is displayed. An OK
   dialog reports completion or error. The installer never deletes or
   overwrites the original ISO or a pre-existing `splanet` directory.
7. Select **Correction du blocage du fondu de palette (FadeLow)**.
   Hover the checkbox to see the detailed explanation.
8. Click **Appliquer**. This applies the tested 4-byte timing fix
   only to the exact retail `SPVOODOO.EXE`, with SHA-256 validation,
   an original backup, and a success/failure OK dialog.
9. Click **Lancer**. The original game's EXE starts in its installation
   directory. A compatible Glide implementation (e.g. separately
   installed nGlide) must be available; this patcher does not
   bundle or install it.

The interface is responsive during extraction and patching through a
worker thread and messages to the GUI. `Appliquer` and `Lancer` are
disabled until the selected installation is ready.

## Patch details

The only supported patch fixes the **palette fade timer starvation /
Go mission-launch hang** on modern CPUs. It has been validated **only
for the original US executable**; on either European edition the FadeLow
checkbox and Apply button remain disabled until a separate patch has been
researched and validated.

| Item | Known verified value |
| --- | --- |
| Supported original SHA-256 | `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a` |
| Output FadeLow SHA-256 | `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45` |
| File offset | `0x489CD` |
| Original bytes | `17 B7 D1 38` |
| Patched bytes | `AC C5 27 37` |
| Backup | `splanet/SPVOODOO.EXE.ScorchedPlanetFix-original.bak` |

The change itself is user-validated on Linux/Wine/nGlide, but this
new graphical Rust program must still be tested on the user's actual
ISO and game before public release. If `Appliquer` is clicked a
second time after success, it recognizes the patched file and
verified backup and makes no change.

## ISO validation scope — important

The code uses the read-only, pure-Rust `isomage` parser to access
ISO9660/Joliet or UDF filesystem entries without mounting the image.

The preflight checks:

- structurally valid filesystem entry extents inside the image;
- bounded entry count and bounded file sizes;
- safe, unambiguous Windows file and directory names;
- presence of `SPLANET/` and `VOODOO/`;
- presence of `GEOMETRY.RWB`, `SPVOODOO.EXE`, `RWDLVOOD.DLL`
  and Smacker `.SMK` video files; `SPLANET.INI` is optional and
  generated when absent;
- **exact SHA-256** of original `SPVOODOO.EXE` and `RWDLVOOD.DLL`
  against verified retail reference binaries;
- streamed extraction of the selected complete `SPLANET` and `VOODOO`
  trees, with individual SHA-256 comparison between bytes read from the
  ISO and bytes written to disk.

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
The original script puts videos under `c:\splanetcd\splanet\video`;
the user tested that **FadeLow runs without the CD folder** after
redirecting the INI `video=` path to the local `splanet\video`.
The patcher incorporates that result by writing its local installation
path rather than an outdated CD-resource path.

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
- NoCD binary patch is **not** included; the local video path was
  sufficient for the owner's tested game startup.
- Rust CD installer research in PR #1 remains separate; this new GUI
  includes only the original 3dfx copy layout and strict validation.
- Do not merge this PR or publish a stable release until the owner
  validates both paths: installed `splanet` and original 3dfx ISO.

Developed by legluondunet, assisted by ChatGPT (OpenAI).
