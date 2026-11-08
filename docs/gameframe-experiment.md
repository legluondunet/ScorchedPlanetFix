# ScorchedPlanetFix v2 — GameFrame Glide

This documents the original GameFrame experiment, now adopted as the **only Glide implementation supported by ScorchedPlanetFix v2**. It includes changes in development `main` after the published v2.0.0 release.

The x64 launcher embeds a Windows x86 GameFrame Glide-to-OpenGL DLL compiled from pinned Apache-2.0 [source](https://github.com/tanepiper/game-frame) at `f195a074a63cc150673c38becdec696163b0b3d1`, with Scorched-specific patches. The original 32-bit game loads the verified DLL as `splanet/glide.dll`.

**Display:** 640×480 original game framebuffer, centered 4:3 scaling in a desktop-sized borderless game window, no display-mode/refresh change, one keyboard/mouse/3D window and 60 FPS cap. On a 1920×1080 logical desktop the rendered image occupies 1440×1080 with two 240-pixel side bars. On Windows, high-DPI scaling can make logical desktop sizes differ from the physical monitor resolution.

**No nGlide compatibility:** external Glide implementations are never loaded, copied from an outside location, or used as fallback. In current post-v2.0.0 development builds, if `splanet/glide.dll` differs from the embedded GameFrame, the launcher **backs up the existing regular file** to an unused `.bak` name, installs its own verified DLL, and logs the original/new hashes and backup path. Existing backups are preserved; symlinks/non-regular files are rejected. The published v2.0.0 release instead required manual removal of an unknown DLL. `--verify` always stays read-only.

**Validation history:** GameFrame integration was maintainer-tested on Wine-staging, GE-Proton and native Windows 11 (installation, second launch, Alt+Tab, videos and gameplay). The v2.0.0 release candidate was separately exercised on Windows 11 and Wine-staging 11.17; CI checks the native x86 Glide ABI, launcher tests and distribution packaging.

**Logging:** the single `ScorchedPlanetFix.log` beside the launcher includes the renderer's Glide API and error diagnostics.

**Licenses:** ScorchedPlanetFix GPL-3.0-or-later, bundled GameFrame Apache-2.0. Original commercial game files and proprietary drivers are never included.
