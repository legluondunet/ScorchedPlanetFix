# ScorchedPlanetFix 0.1.0 — FadeLow (Rust)

A small native, **dependency-free** compatibility patcher for the 1996
3dfx Windows edition of Scorched Planet. It fixes the freeze encountered
when pressing **Go** on some modern CPU/Wine/nGlide configurations.

**Status:** the 4-byte FadeLow change has been confirmed in gameplay by
the project owner under Linux/Wine/nGlide. The **new Rust implementation**
must pass its Linux and Windows CI tests and still requires a normal
owner-side check before publishing a final release.

**This project does NOT include the game, commercial DLLs, audio, images,
assets or a modified SPVOODOO.EXE.** Users must provide a legitimate
copy of their own original executable. It makes no network calls.

## What is patched?

Only one known retail 3dfx EXE is supported:

| Item | Value |
| --- | --- |
| Original file | SPVOODOO.EXE (PE32 / i386), size 836608 bytes |
| Original SHA-256 | fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a |
| FadeLow SHA-256 | 0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45 |
| PE VA | 0x004495CD |
| File offset | 0x489CD |
| Original bytes | 17 B7 D1 38 |
| Patched bytes | AC C5 27 37 |
| Change | Lowers the fade timer's float32 threshold from approximately 0.0001 to 0.00001 |

The EXE source hash, PE32 headers/section offset, original bytes, output
hash and the temporary output file are verified. **Other editions,
unrecognized EXEs and already-modified EXEs are refused**, rather than
being patched speculatively.

## Download and run

Build artifacts are produced for Windows x86_64 and Linux x86_64 in the
[GitHub Actions workflow](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml).

Download the artifact matching your operating system from a **successful**
workflow run. It contains the patcher executable, this README, its SHA-256
and the GPL license. **No Python or Rust installation is necessary** to
run a downloaded executable.

The patcher is a command-line program. Close the game before modifying
its executable, and use a terminal in the game's folder, or provide full
file paths as shown below. Administrator privileges are not normally
needed if you can write the game directory.

### Linux / Wine / Lutris (recommended non-destructive method)

Assuming you extracted the Linux binary somewhere on your disk:

```bash
chmod +x ./scorched-planet-fix
./scorched-planet-fix status --exe "/mnt/LUTRIS/Games/scorched-planet/drive_c/splanet/SPVOODOO.EXE"
./scorched-planet-fix copy \
  --exe "/mnt/LUTRIS/Games/scorched-planet/drive_c/splanet/SPVOODOO.EXE" \
  --output "/mnt/LUTRIS/Games/scorched-planet/drive_c/splanet/SPVOODOO-FadeLow.EXE"
```

In **Lutris → Configure → Game options**, select the new
`SPVOODOO-FadeLow.EXE` as the executable. Keep the same working
directory, Wine prefix and nGlide setup. To revert this method, point
Lutris at the unchanged original `SPVOODOO.EXE`. Do not repeat
`copy` over a file that already exists: overwrites are refused.

### Windows (PowerShell)

Run the Windows executable (extracted from its ZIP):

```powershell
.\scorched-planet-fix.exe status --exe "C:\Games\ScorchedPlanet\SPVOODOO.EXE"
.\scorched-planet-fix.exe copy --exe "C:\Games\ScorchedPlanet\SPVOODOO.EXE" --output "C:\Games\ScorchedPlanet\SPVOODOO-FadeLow.EXE"
```

Start the new `SPVOODOO-FadeLow.EXE` normally, with the game assets and
driver DLLs already present in the installation folder.

### Optional: patch the original filename, with verified backup

Some launchers may require the original filename. In that case, run
(with the game closed):

```bash
./scorched-planet-fix apply --exe "/path/to/SPVOODOO.EXE" --yes
./scorched-planet-fix status --exe "/path/to/SPVOODOO.EXE"
```

The patcher creates **before installation**:

```text
SPVOODOO.EXE.ScorchedPlanetFix-original.bak
```

It will never overwrite an existing backup. If the backup exists, it
must have the exact original SHA-256. Applying twice is idempotent if
the backup is present. A temporary file is prepared and checked before
replacement of the installed executable in the same directory.

To undo, with the game closed:

```bash
./scorched-planet-fix restore --exe "/path/to/SPVOODOO.EXE" --yes
```

`restore` accepts only a verified original backup and the exact
known FadeLow EXE. The backup is preserved after restoration.

On Windows, a running EXE or read-only game directory may cause
replacement to fail safely. Use the non-destructive `copy` approach
or adjust your installation's permissions. Keep the original backup.

### CLI reference

```text
scorched-planet-fix --help
scorched-planet-fix --version
scorched-planet-fix status  [--exe SPVOODOO.EXE]
scorched-planet-fix copy    [--exe SPVOODOO.EXE] --output SPVOODOO-FadeLow.EXE
scorched-planet-fix apply   [--exe SPVOODOO.EXE] --yes
scorched-planet-fix restore [--exe SPVOODOO.EXE] --yes
```

For the default relative EXE path, first `cd` into the game folder.

## Important boundaries

- This build contains the **FadeLow timing fix only**.
- **No NoCD patch**: the owner confirmed FadeLow itself works with
  the original CD data directory disabled after adjusting
  `splanet.ini` to `video=c:\splanet\video` and ensuring that
  necessary resources are stored locally.
- No graphics-resolution modifications, RenderWare patches or
  Glide replacements are included. All existing high-resolution
  research is parked in [draft PR #4](https://github.com/legluondunet/ScorchedPlanetFix/pull/4).
- The separate Rust CD installer is in [draft PR #1](https://github.com/legluondunet/ScorchedPlanetFix/pull/1)
  and is **not** this patcher.
- A Windows-only game runtime was **not** independently tested by the
  author of the patcher. The validated game behavior comes from the
  owner's Linux/Wine/nGlide test.
- Windows SmartScreen may warn on an unsigned, unfamiliar executable.
  Verify the downloaded ZIP and source before allowing execution.

## Build from source

Rust 1.76 or newer:

```bash
cargo test --locked
cargo build --release --locked
```

Binary: `target/release/scorched-planet-fix` (or `.exe` on Windows).

The source has no runtime or build-time crates from third parties.
The SHA-256 routine has FIPS standard-vector tests, PE32 parsing tests,
four-byte-only patch tests, and synthetic file copy/backup/rollback
tests. The Windows and Linux CI workflows run them without distributing
or checking in copyrighted game content.

This program is licensed **GPL-3.0-or-later**; see `LICENSE`.
New code copyright legluondunet, assisted by ChatGPT (OpenAI).

## Release quality gate

Before declaring a production release, ensure:

1. Linux and Windows workflows finish with success.
2. Both build archives contain binaries and SHA-256 lists.
3. On a separate copy of the owner's original 3dfx EXE, the Rust
   patcher produces SHA-256
   `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45`.
4. It launches the game, reaches gameplay and does not regress FadeLow.
5. A backup/apply/restore cycle returns exactly the retail SHA-256.
6. The owner **explicitly approves** merging and publishing a release.

Until then the branch and PR remain a **draft**.
