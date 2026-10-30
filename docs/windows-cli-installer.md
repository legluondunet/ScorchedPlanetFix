# ScorchedPlanetFix — Windows x64 command-line installer

This is a **console-only** program. It has no graphical user interface. The 32-bit game is not bundled.

## Commands

```powershell
.\ScorchedPlanetFix.exe
.\ScorchedPlanetFix.exe --nogui
.\ScorchedPlanetFix.exe --nogui "D:\ISO\Scorched Planet.iso"
.\ScorchedPlanetFix.exe --nogui "D:\ISO\Scorched Planet.cue"
.\ScorchedPlanetFix.exe --nogui "D:\ISO\Scorched Planet.cue" "D:\ISO\Scorched Planet.bin"
.\ScorchedPlanetFix.exe --verify
.\ScorchedPlanetFix.exe --launch
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe --version
```

- No arguments and `--nogui`: extract a single ISO/CUE into **current working directory**; if there is already `splanet`, verify and patch its recognized EXE and normalize its INI without overwriting the game assets. The `--nogui` name is retained only as a compatibility alias.
- `--verify`: read-only check of the `splanet` directory **next to ScorchedPlanetFix.exe**; validates patch and `.bak`, all immutable files' SHA-256, `splanet.ini`, local `glide.dll`. Does not overwrite savegames or mutable configuration.
- `--launch`: run **the same full verification first**, then launch the game from `splanet` and monitor its exit status.
- `--help`: commands and version; `--version`: version only.
- Any failed operation emits an error and writes it to `ScorchedPlanetFix.log` (fallback logging attempted when startup fails).

The installer supports original US, European EN/ES/IT, and FR/DE versions from the owner's inventories. It unpacks `SPLANET/` and `VOODOO/` from ISO or CUE/BIN; copies all game assets from `SPLANET/` and only `spvoodoo.exe` and `rwdlvood.dll` from `VOODOO/`, **excluding** original `glide.dll`, `sst1init.dll` and `fxmemmap.vxd`. Normalizes names to lowercase, verifies copied data, saves `spvoodoo.exe.bak`, applies the edition patch, and writes:

```ini
[scorch]
video= video
language=1
HighQualityVideo=1
```

Language 1 is the default for US and EN/ES/IT (Italian=4, Spanish=5); FR/DE defaults to French=2 (German=3). This example is not the FR/DE default.

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
