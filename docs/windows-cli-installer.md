# ScorchedPlanetFix — Windows x64 command-line installer

This is a **console-only** program. It has no graphical user interface. The 32-bit game is not bundled.

## Commands

```powershell
.\ScorchedPlanetFix.exe
.\ScorchedPlanetFix.exe "D:\ISO\Scorched Planet.iso"
.\ScorchedPlanetFix.exe "D:\ISO\Scorched Planet.cue"
.\ScorchedPlanetFix.exe "D:\ISO\Scorched Planet.cue" "D:\ISO\Scorched Planet.bin"
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --launch
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe --version
```

- No arguments: extract a single ISO/CUE found in the **current working directory**; if there is already `splanet`, verify and patch its recognized EXE and normalize its INI without overwriting the game assets. To choose media explicitly, provide `<image.iso>` or `<image.cue> [track.bin]` as positional arguments. There is no GUI and no GUI-related command-line switch.
- `--verify`: read-only check of the `splanet` directory **next to ScorchedPlanetFix.exe**; validates patch and `.bak`, all immutable files' SHA-256, `splanet.ini`, local `glide.dll`. Does not overwrite savegames or mutable configuration.
- `--launch`: run **the same full verification first**, then launch the game from `splanet` and monitor its exit status.
- `--help`: commands and version; `--version`: version only.
- Any failed operation emits an error and writes it to `ScorchedPlanetFix.log` (fallback logging attempted when startup fails).

The installer supports original US, European EN/ES/IT, and FR/DE versions from the owner's inventories. It unpacks `SPLANET/` and `VOODOO/` from ISO or CUE/BIN; copies all game assets from `SPLANET/` and only `spvoodoo.exe` and `rwdlvood.dll` from `VOODOO/`, **excluding** original `glide.dll`, `sst1init.dll` and `fxmemmap.vxd`, and also **omitting unused `SPLANET/SPLANETW.EXE`** from all three editions. When invoked without arguments for an existing installation, the CLI removes an old `splanetw.exe` only after matching its SHA-256 to that edition's original CD inventory. It refuses to delete a modified or unknown file. `--verify` requires the unused executable to be absent. Normalizes names to lowercase, verifies copied data, saves `spvoodoo.exe.bak`, applies the edition patch, and writes:

```ini
[scorch]
video= video
language=1
HighQualityVideo=1
```

Language 1 is the default for US and EN/ES/IT (Italian=4, Spanish=5); FR/DE defaults to French=2 (German=3). This example is not the FR/DE default.

## Owner-validated CUE/BIN editions

On 2026-10-30, the owner reported successful CUE/BIN installation and **no CD-required message** for both US 1.08 and FR/DE 1.06. The original source images are preserved. This is user-reported functional validation, not an exhaustive test of every mission or platform. EN/ES/IT CUE/BIN extraction and `--verify` also passed the owner's test, but the game displayed a CD-required message with FadeLow-only; the new six-byte combined patch has now been validated by the owner in gameplay without a CD.

## EN/ES/IT CD-media check research and safe migration

The owner has confirmed EN/ES/IT CUE/BIN extraction and `--verify` work,
but the four-byte FadeLow build still displayed **NO CD** in gameplay.
Static analysis of the exact original EN/ES/IT executable
(SHA-256 `9ab6442553d055a14273ee0e3c5a912986662dc0a3cc8512ac716879b5068e5f`)
located the same drive-search/failed-media-return routine as FR/DE:
at **VA `0x43562B` / file offset `0x34A2B`**,
the failure-return instruction `33 C0` (`xor eax,eax`) can be
changed to `B0 01` (`mov al,1`). This changes exactly two bytes.
Combined with four FadeLow bytes at `0x487AD`, the target SHA-256
is `66c705a77d290d4bed6abdb4b8b065337aaeb50f9e4480090bbe2dfd5006d35c`.
**The owner has validated the combined EN/ES/IT patch in the tested no-CD gameplay scenario.**
This is an edition-specific patch, not the US or FR/DE offsets.

The old four-byte EN/ES/IT result with SHA-256
`dec3977ff83758f3a65bc5fe96a6bafaa064bbf34ef93ee9f35593fb65e173bc`
is recognized for safe migration. Run the installer with **no arguments**
in the directory containing `splanet`. The six-byte candidate is
reconstructed only from the **SHA-256-verified original `spvoodoo.exe.bak`**
and replaces the old executable atomically; saves and the original backup
are retained. Missing, corrupted or mismatched backups lead to refusal.

Run `--verify` and `--launch` using the new build **without mounting a CD**,
and confirm the menu, a mission, videos, audio, saves and game exit.
The owner has verified this EN/ES/IT no-CD scenario; do not merge the draft PR without explicit approval.

## US no-CD patch and safe upgrades

The owner's fresh US 1.08 installation ran successfully through extraction,
FadeLow and immutable-file SHA-256 verification but displayed **NO CD** at
game launch; changing the INI video path to an absolute local path did not
fix it. Earlier experiments with the same edition sometimes launched without
the CD, so the media check was omitted from the first release candidate.
The US combined patch changes six bytes: FadeLow four bytes
at raw offset `0x489CD` and the separately researched CD-media failure
result two bytes at `0x34C1B` (`33 C0` -> `B0 01`). Its exact
expected SHA-256 is
`4dc4de1f9034e17b266da7b64a83ba897b4dca9e8cf9a8654f5988d02d1e0613`.
**The owner has now confirmed that the US combined patch works in the tested environment.**

Previously installed US executables with exact earlier FadeLow hash
`0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45`
are recognized by the patch engine. Rerun the CLI installer with no
argument **from the directory already containing `splanet`** to migrate
them. Migration is refused if `spvoodoo.exe.bak` is missing, corrupt,
or does not match the exact original US retail hash. The new six-byte
candidate is deterministically rebuilt from the verified original backup
and atomically replaces the old FadeLow executable. The original backup
and game saves are preserved. `--verify` deliberately refuses the
previous US FadeLow-only result once the six-byte profile is selected.

Test `--verify` followed by `--launch`, then ensure the menu, Go/mission,
videos, sound, save/load, and normal exit all work **without any optical
CD image mounted**. If the game still reports NO CD, investigate file
attributes/drive-root mapping instead of blindly applying more patches.
The US patch is owner-validated; the overall PR still requires final review and explicit permission to merge.

## nGlide dependency and GPL boundary

**Do not include nGlide in our GPL archive.** nGlide v2.10 is proprietary freeware from Zeus Software; free download does not imply permission to redistribute.

Official download: https://www.zeus-software.com/downloads/nglide

The user downloads and installs it separately in the same Windows/Wine environment. The tool may copy a **user-installed** `glide.dll` (32-bit PE DLL) from the patcher's folder or Windows `SysWOW64` to `splanet`. If missing, installation can finish with a warning, but `--verify` and `--launch` refuse to proceed until `splanet/glide.dll` is available. This PE check does not cryptographically authenticate the nGlide publisher.

The launcher's per-process environment settings override **only this game's nGlide configuration**; the system-wide registry is *not modified*. Zeus Software documents this precedence: https://www.zeus-software.com/forum/viewtopic.php?p=4254

- `NGLIDE_BACKEND=2` (Vulkan when the Vulkan loader is found), else `0` (Automatic)
- `NGLIDE_RESOLUTION=1` (desktop)
- `NGLIDE_ASPECT=1` (correct aspect ratio)
- `NGLIDE_REFRESH=2` (60 Hz)
- `NGLIDE_VSYNC=1` (on, initially chosen to reduce tearing/unstable speed)
- `NGLIDE_GAMMA=5` (1.0)
- `NGLIDE_SPLASH=0` (off)

When the Vulkan loader is present but the driver is broken, nGlide may still fail. Test game behavior and nGlide settings in real Windows/Wine environments before final release.

See the main [README](../README.md), [CD edition profiles](cd-editions.md) and [FadeLow research](european-fadelow-research.md). No proprietary file is committed to GitHub. No changes to `main` or merges without owner approval.
