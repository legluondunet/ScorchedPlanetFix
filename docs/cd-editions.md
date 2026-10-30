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
- Extract only the original `SPLANET/` and `VOODOO/` trees into an isolated temporary workspace; install the full `SPLANET/` tree plus `VOODOO/SPVOODOO.EXE` and `VOODOO/RWDLVOOD.DLL` only. Normalize all resulting names to lowercase and refuse case-fold collisions. Exclude `glide.dll`, `sst1init.dll` and `fxmemmap.vxd`. Create and verify `spvoodoo.exe.bak`, patch, and finally generate `splanet.ini` with local videos.
- **Validated patch combinations:** US 1.08: FadeLow only (4 bytes); EN/ES/IT 1.06: FadeLow only (4 bytes); FR/DE 1.06: FadeLow plus a separate, edition-specific CD-media result change (6 bytes). The owner confirmed the combined FR/DE candidate works in gameplay on 2026-10-30 after the FadeLow-only candidate reached a "PAS DE CD" menu. This does **not** establish that standalone FR/DE FadeLow is sufficient, that physical CD media are never needed for any gameplay resource, or that the automated GUI installer has already been tested.
- The complete inventoried per-file hashes are preserved in `data/cd-inventories/` for future media verification, but the current installer verifies only required binary/resource identities and checks extracted file copies against the source ISO. It does *not yet* compare all CD files to these manifests.
- No fully authenticated retail-master provenance or end-to-end European installation/playability test is claimed. All changes remain in draft PR #5 until separately approved.

Developed by legluondunet, assisted by ChatGPT (OpenAI).

## Language mapping determined from the original game executables

A PE32 data table of pointers to language resource names is indexed as follows: `1=English`, `2=French`, `3=German`, `4=Italian`, `5=Spanish` (index zero is null). The owner-provided tested EN/ES/IT `splanet.ini` has `language=4` (Italian). The GUI offers only languages for which the CD inventory has a corresponding `SPLANET/*.REZ` resource:

- US: English (1)
- EN/ES/IT: English (1), Italian (4), Spanish (5)
- FR/DE: French (2), German (3)

After the game EXE is backed up and patched, the installer sets `video=` to the local installation folder and `HighQualityVideo=1`, adding the edition default `language=`. Existing keymapping, gamma, joystick, and other personal options are preserved. The complete original GUI install path has not yet been user-validated.
