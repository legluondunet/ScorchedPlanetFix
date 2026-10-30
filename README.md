# ScorchedPlanetFix

Open-source compatibility tools for **Scorched Planet (Criterion Studios, 1996)**. The original commercial game files are **not** redistributed.

## Scorched Planet Setup 0.1 — development preview

A native Rust/Windows installer that reads the **files copied from the original CD into a folder**. The user selects a source folder containing `SPLANET` and `VOODOO`, an empty destination, and French or German.

The GUI displays original InstallShield artwork (BMP) **when those files are accessible** from the user's source folder; some editions pack artwork inside InstallShield archives, not yet extracted.

It copies game files and 3dfx edition assets, preserves subdirectories, writes local paths in `SPLANET.INI`, and provides a progress indicator. It **does not execute** the Windows 95 installer, write registry data, or launch DirectX 1996 setup.

**Known limitations:** `COMPRESS.BIN` is not unpacked yet; runtime CD checks and nGlide/dgVoodoo/RenderWare crashes are not resolved. Do not treat this as a tested or playable release.

### Build for Windows 11

A CI artifact named **ScorchedPlanetSetup-Windows-development** is produced by the Windows installer workflow once compilation and tests pass. Alternatively, from the repository root:

```powershell
cargo build --release
.\target\release\scorched-planet-setup.exe
```

Choose the copied *CD root*, not the `INSTALL\WINDOWS` subfolder. By default, installation targets `C:\Games\ScorchedPlanet`; the destination must not already exist.

See [installer design and remaining validation](docs/installer.md).

### Contributions

Development takes place in **draft pull requests**; nothing is merged before build and on-machine validation. New source is intended to be licensed GPL-3.0-or-later after the repository license is finalized.

*legluondunet assisted by ChatGPT (OpenAI)*

### Original CD inspection

The setup screen includes **Analyser le contenu du CD…**, which saves
`ScorchedPlanet-CD-audit.txt`. This read-only report lists source file names and
sizes and small textual `SETUP.INI` and `SPLANET.INI` files. It helps reconstruct the original
InstallShield file map and investigate language-specific resources without modifying
the source CD. See [installer audit instructions](docs/installer.md).

The audit does **not** yet prove complete installation or English language support
for a particular original disc.
