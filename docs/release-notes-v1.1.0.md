# ScorchedPlanetFix v1.1.0 — Release notes

## Changes since v1.0.0

ScorchedPlanetFix is a GPL-3.0-or-later Windows x64 console installer, patcher, verifier and launcher for the 1996 PC game *Scorched Planet*. It can also run through Wine on Linux. The original game remains 32-bit.

- **Fourth disc variant:** Added the alternative European FR/DE 1.06 ISO. Its 3dfx executable uses new exact SHA-256 fingerprints, while game resources match the existing FR/DE version. The original and patched executable hashes are checked independently.
- **Safer, smaller game installations:** Do not install the unused `splanet.exe` and `splanetw.exe` software renderers. On older installations these are deleted **only** if each matches the precise original retail file hash. Modified or unrecognized files remain untouched.
- **Optional 3dfx startup logo:** `--launch --3dfx-logo` enables per-process `NGLIDE_SPLASH=1`. The separate 32-bit `3DfxSpl.dll` must be copied **manually** into the game's `splanet/` folder beside `glide.dll`. A missing or invalid module produces an explanatory error; there is no automatic copying or downloading.
- **Unchanged default behavior:** `--launch` keeps the logo off. `--verify` does not inspect, hash or require the optional `3DfxSpl.dll`.

### Supported original game discs

| Version | Languages | Validation |
| --- | --- | --- |
| US 1.08 | English | Owner-tested ISO/CUE-BIN, FadeLow, No-CD and Wine gameplay |
| Europe EN/ES/IT 1.06 | English, Spanish, Italian | Owner-tested ISO/CUE-BIN, FadeLow, No-CD and Wine gameplay |
| Europe FR/DE 1.06 | French, German | Owner-tested ISO/CUE-BIN, FadeLow, No-CD and Wine gameplay |
| Alternate Europe FR/DE 1.06 | French, German | Owner-tested **ISO**, FadeLow, No-CD, videos, menus and Wine/GE-Proton gameplay |

The fourth row describes a different *binary variant*, not a new language set. Its original ISO was compared file-by-file against the previously known FR/DE image: five executables differ, while the remaining 571 file records match. These findings do not establish universal game or driver compatibility.

### Tests and known limits

The alternate FR/DE ISO was installed and played by the owner under Wine/GE-Proton on 2026-10-31. Its pre-launch read-only audit verified 126 immutable files, including the game EXE and original `.bak`; videos and FadeLow worked, and the owner quit voluntarily after normal gameplay. The game process returned status `1`; the launcher still reports this nonzero status without assuming that it is a crash.

Windows GitHub Actions runs all Rust tests, compiles a Windows x64 console executable, checks its architecture, packages it and verifies the SHA-256 manifest. Native Windows 11 tests of the core features were reported previously, but this **new alternate image** has not undergone exhaustive native Windows or multi-mission testing.

### Download, install and launch

Once the release is published, download `ScorchedPlanetFix-v1.1.0-Windows-x64.zip` and its accompanying `.zip.sha256` from the GitHub v1.1.0 release page. The ZIP includes `ScorchedPlanetFix.exe`, `README.md`, `LICENSE.txt` and `SHA256SUMS.txt` (the executable SHA-256).

Place your own supported ISO or CUE/BIN beside the extracted patcher and separately install nGlide or provide your own 32-bit `glide.dll`. Then run in PowerShell (prepend `wine` under Linux):

```powershell
.\ScorchedPlanetFix.exe
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --launch
```

If you want the optional 3dfx logo, manually place your own `3DfxSpl.dll` in `splanet/` and use `--launch --3dfx-logo`.

The v1.1.0 release is created only by an explicit manual workflow from protected `main`; preparing or merging this change does **not** publish a release or modify the existing v1.0.0 tag.

See the [README](../README.md) and [CD edition fingerprints](cd-editions.md).

**Licensing:** GPL-3.0-or-later. No copyrighted game data, original game executables or proprietary nGlide DLLs are distributed.

**Credits:** legluondunet, assisted by ChatGPT (OpenAI). Independent community project, unaffiliated with the respective rights holders.
