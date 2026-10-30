# Scorched Planet — CD edition fingerprints

Source: owner-supplied file inventories created from three distinct original-media file trees, recorded 2026-10-30. These are **reference fingerprints**, not independently authenticated publisher retail-master manifests. They contain file names, byte sizes and hashes, but no proprietary binaries or game assets.

| Edition | `autorun.inf` InstallKey suffix | Files | Directories | Total file bytes | SMK files | `VOODOO/SPVOODOO.EXE` (SHA-256) | `SPLANET/GEOMETRY.RWB` (SHA-256) |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| US | 1.08 | 492 | 34 | 495706103 | 58 | `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a` | `85e153739a5c43e514e2a8146d0cdfccb2c998d73e1494cfd45e1aa05f11531e` |
| EN/ES/IT | 1.06 | 576 | 41 | 656149792 | 116 | `9ab6442553d055a14273ee0e3c5a912986662dc0a3cc8512ac716879b5068e5f` | `29b3d8f87eed27ca933b81231b84f7e406506b498e5fc74bfee52f5b1004fb1b` |
| FR/DE | 1.06 | 576 | 42 | 597319505 | 87 | `eff38cedf2408566060b3c7cf39be59c54d23e67b0a4c7cb0725051aa87e1b72` | `a2dc1882d66f0d7607a955c8bf9ad28282c3d1d08ca0675a90ca93ecd20f6c69` |

In all three editions, `VOODOO/RWDLVOOD.DLL` is **identical** (SHA-256 `1bd95e0dfeaaa47c53d5e2d4206027968790b928949321efd1eb2bb70f9201de`). All three have no `SPLANET/SPLANET.INI`; the installer generates this configuration. The two European editions each include `INSTALL/DOS/SPLANET.INI` (a different location), which does **not** mean an installed `splanet.ini` already exists.

**Important:** the numeric 1.06/1.08 values above are derived from `autorun.inf` `InstallKey`; they are not independently verified game-executable version resources. The FR/DE inventory additionally declares `FrenchInstall=1`.

European editions include three legacy driver files in the `VOODOO` tree (`glide.dll`, `fxmemmap.vxd`, `sst1init.dll`); the US inventory's `VOODOO` tree does not. Presence in the original CD must not be interpreted as compatibility with current Windows or Wine. The modern installer deliberately omits these three driver files while preserving the source ISO and all actual game resources. The installation policy still needs real end-to-end tests with an external suitable Glide implementation.

## Integration and validation boundary

- Identify an edition using exact SHA-256 of `VOODOO/SPVOODOO.EXE`, `SPLANET/GEOMETRY.RWB` and the shared RenderWare driver. Refuse unrecognized or mixed binaries.
- Extract only the original `SPLANET/` and `VOODOO/` trees into an isolated temporary workspace; install the full `SPLANET/` tree plus `VOODOO/SPVOODOO.EXE` and `VOODOO/RWDLVOOD.DLL` only. Normalize all resulting names to lowercase and refuse case-fold collisions. Exclude `glide.dll`, `sst1init.dll` and `fxmemmap.vxd` as well as the unused `SPLANET/SPLANETW.EXE` software renderer (all three editions). Older installed copies of `splanetw.exe` are removed by the normal no-argument installer only after exact SHA-256 and edition checks; `--verify` rejects an installation retaining this obsolete executable. Create and verify `spvoodoo.exe.bak`, patch, and finally generate `splanet.ini` with local videos.
- **Owner's CUE/BIN acceptance tests (2026-10-30):** US 1.08 and FR/DE 1.06 successfully installed from their CUE/BIN media, launched without a CD-required message, and were explicitly approved by the owner. EN/ES/IT CUE/BIN extraction and `--verify` have since been validated; gameplay still showed a CD-required message with the earlier four-byte FadeLow patch. These tests confirm the reported no-CD issue is resolved in both tested editions but do not imply all missions or native Windows environments have been exhaustively tested.
- **Patch combinations / gameplay validation:** **US 1.08: FadeLow + CD-media check fix (6 bytes; owner-validated)**, previously FadeLow only (4 bytes). A newly installed US copy under Wine 11.17 displayed **NO CD** with FadeLow-only, even after the user changed the INI to an absolute video path. **EN/ES/IT 1.06: FadeLow + experimental CD-media check (6 bytes; gameplay retest pending)**. The previous four-byte FadeLow-only build extracted and verified correctly but displayed a CD-required message in the owner's CUE/BIN game test. **FR/DE 1.06: FadeLow + its own edition-specific CD-media change (6 bytes, owner-tested)**. The owner subsequently confirmed that the US combined patch works in their environment; this does not imply that all later missions or every media resource has been tested. All originals remain stored intact as `.bak`. Legacy US or EN/ES/IT FadeLow-only copies can be migrated from their hash-verified original `.bak` by rerunning the console installer with no image argument.
- Original-CD per-file SHA-256 inventories are retained in `data/cd-inventories/` and compiled as `src/verify_manifest.rs` for **read-only `--verify` and pre-launch audits** of immutable installed game assets. The audits also require the exact patched executable, matching original `.bak`, semantically valid INI (`video= video`, valid edition language) and skip mutable `.sav` slots. Installer preflight separately validates critical original EXE/DLL/geometry fingerprints and hashes source-to-temporary-to-install copy data. These fingerprints are owner-supplied, not authenticated publisher retail-master hashes.
- These owner-supplied manifests are not independently authenticated publisher retail masters; the new console install/launch workflow is not yet end-to-end validated. All changes remain in draft PR #5 until separately approved.

Developed by legluondunet, assisted by ChatGPT (OpenAI).

## Language mapping determined from the original game executables

A PE32 data table of pointers to language resource names is indexed as follows: `1=English`, `2=French`, `3=German`, `4=Italian`, `5=Spanish` (index zero is null). The owner-provided tested EN/ES/IT `splanet.ini` has `language=4` (Italian). The installer supports only languages for which the CD inventory has a corresponding `SPLANET/*.REZ` resource:

- US: English (1)
- EN/ES/IT: English (1), Italian (4), Spanish (5)
- FR/DE: French (2), German (3)

After the game EXE is backed up and patched, the installer writes `[scorch]` and the owner-tested portable `video= video` setting, together with `HighQualityVideo=1` and the edition default `language=`. Existing keymapping, gamma, joystick, and other personal options are preserved. The new console installation and launch workflow still requires end-to-end testing.

## Console-only nGlide dependency

The original CD's `glide.dll` is excluded because it is an obsolete Voodoo hardware driver. The Windows x64 **console** patcher does not bundle nGlide (third-party proprietary freeware). The game requires a **user-provided local 32-bit `splanet/glide.dll`**, which the tool may copy from the user's own installation; users download nGlide themselves from https://www.zeus-software.com/downloads/nglide . `--verify` and `--launch` check its presence. The program applies per-game nGlide environment variables instead of globally changing the Windows/Wine registry.
