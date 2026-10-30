# ScorchedPlanetFix

**A native Rust compatibility patcher for Scorched Planet (Criterion Studios, 1996).**

The **FadeLow** change fixes the game freezing when entering a mission
(on some modern systems, including the owner's tested Wine/nGlide setup).
The fix changes **four bytes** in the **exact supported original 3dfx EXE**.
It was tested successfully in the game by the owner.

**No Python, Wine (for patching), additional runtime, internet connection,
CD copy or modified game binary is distributed.** Users need their own
original `SPVOODOO.EXE`.

## Download

The latest successful development builds are available as Windows and Linux
archives from the [Rust FadeLow GitHub Actions workflow](https://github.com/legluondunet/ScorchedPlanetFix/actions/workflows/rust-fadelow-patcher.yml).

The workflow produces:

- `ScorchedPlanetFix-v0.1.0-Windows-x64.zip` containing a Windows patcher EXE.
- `ScorchedPlanetFix-v0.1.0-Linux-x64.tar.gz` containing a Linux patcher binary.

**Development builds are not an approved public release until the owner
validates the Rust binary on the original game and authorizes merging.**

## Quick start

With a downloaded, extracted executable, use your own original game file.

**Linux / Lutris** (copy-only, recommended):

```bash
./scorched-planet-fix copy --exe "/path/to/SPVOODOO.EXE" --output "/path/to/SPVOODOO-FadeLow.EXE"
```

**Windows** (PowerShell, copy-only):

```powershell
.\scorched-planet-fix.exe copy --exe "C:\Games\ScorchedPlanet\SPVOODOO.EXE" --output "C:\Games\ScorchedPlanet\SPVOODOO-FadeLow.EXE"
```

Choose the newly generated executable in Lutris or launch it from
the original game folder. The original EXE is unchanged.

Also supported: `status`, `apply --yes` with a verified original
backup, and `restore --yes`. Unknown versions are always refused.

**Read the [complete Windows/Linux usage and safety guide](docs/rust-fadelow-patcher.md)** before using in-place `apply`.

## Separate workstreams

- [FadeLow validation and older Python research (draft PR #2)](https://github.com/legluondunet/ScorchedPlanetFix/pull/2).
- [Original CD check investigation (draft PR #3)](https://github.com/legluondunet/ScorchedPlanetFix/pull/3).
  The owner confirmed **no NoCD binary modification was needed**
  after redirecting `splanet.ini` to the locally installed videos.
- [Native-resolution research (draft PR #4)](https://github.com/legluondunet/ScorchedPlanetFix/pull/4).
  **Parked**: all technical findings are retained in its documentation;
  no resolution changes are part of this patcher.
- [Separate Rust CD installer (draft PR #1)](https://github.com/legluondunet/ScorchedPlanetFix/pull/1).
  Not part of the FadeLow patcher.

## Build

Rust 1.76+ is sufficient; there are **no third-party crates**:

```bash
cargo test --locked --all-targets
cargo build --locked --release
```

The release executable is `target/release/scorched-planet-fix`
(`.exe` under Windows). The source is **GPL-3.0-or-later**.
Original retail game and proprietary modules are never bundled.

Developed by legluondunet, assisted by ChatGPT (OpenAI).
