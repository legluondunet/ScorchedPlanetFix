# ScorchedPlanetFix v2 — GameFrame Glide

This documents the original GameFrame experiment, now adopted as the **only Glide implementation supported by ScorchedPlanetFix v2**. It includes the improvements introduced in v2.0.1 after the historical v2.0.0 release.

The x64 launcher embeds a Windows x86 GameFrame Glide-to-OpenGL DLL compiled from pinned Apache-2.0 [source](https://github.com/tanepiper/game-frame) at `f195a074a63cc150673c38becdec696163b0b3d1`, with Scorched-specific patches. The original 32-bit game loads the verified DLL as `splanet/glide.dll`.

**Display:** 640×480 original game framebuffer, centered 4:3 scaling in a desktop-sized borderless game window, no display-mode/refresh change, one keyboard/mouse/3D window and 60 FPS cap. On a 1920×1080 logical desktop the rendered image occupies 1440×1080 with two 240-pixel side bars. On Windows, high-DPI scaling can make logical desktop sizes differ from the physical monitor resolution.

**No nGlide compatibility:** external Glide implementations are never loaded, copied from an outside location, or used as fallback. In v2.0.1, if `splanet/glide.dll` differs from the embedded GameFrame, the launcher **moves the existing regular file** to the single fixed `glide.dll.bak` (replacing a previous regular backup), installs its verified DLL, and logs both hashes and backup replacement. This prevents backup proliferation; earlier `.bak` contents are intentionally overwritten. Symlinks/non-regular files are rejected. The published v2.0.0 release instead required manual removal of an unknown DLL. `--verify` always stays read-only.

**Validation history:** GameFrame integration was maintainer-tested on Wine-staging, GE-Proton and native Windows 11 (installation, second launch, Alt+Tab, videos and gameplay). The v2.0.0 release candidate was separately exercised on Windows 11 and Wine-staging 11.17; CI checks the native x86 Glide ABI, launcher tests and distribution packaging.

**Logging (v2.0.2 and later):** the launcher appends installation, file-validation, hardware and process diagnostics to `ScorchedPlanetFix_installer.log`. The GameFrame DLL appends Glide/OpenGL diagnostics, menu telemetry and initialization errors to `ScorchedPlanetFix.log`. Both files are beside the launcher. A unique `session_id` is written into both logs for each game launch, even if the DLL fails before rendering. Existing unified `ScorchedPlanetFix.log` contents are not discarded; newer DLL sessions are marked with `SCORCHED_SESSION`. The historical v2.0.1 binary used the earlier unified logging scheme.

**Licenses:** ScorchedPlanetFix GPL-3.0-or-later, bundled GameFrame Apache-2.0. Original commercial game files and proprietary drivers are never included.
