# ScorchedPlanetFix v2 — GameFrame Glide

This documents the original GameFrame experiment, now adopted as the **only Glide implementation supported by ScorchedPlanetFix v2**. It includes the improvements introduced in v2.0.1 after the historical v2.0.0 release.

The x64 launcher embeds a Windows x86 GameFrame Glide-to-OpenGL DLL compiled from pinned Apache-2.0 [source](https://github.com/tanepiper/game-frame) at `f195a074a63cc150673c38becdec696163b0b3d1`, with Scorched-specific patches. The original 32-bit game loads the verified DLL as `splanet/glide.dll`.

**Display:** 640×480 original game framebuffer, centered 4:3 scaling in a desktop-sized borderless game window, no display-mode/refresh change, one keyboard/mouse/3D window and 60 FPS cap. On a 1920×1080 logical desktop the rendered image occupies 1440×1080 with two 240-pixel side bars. On Windows, high-DPI scaling can make logical desktop sizes differ from the physical monitor resolution.

**No nGlide compatibility:** external Glide implementations are never loaded, copied from an outside location, or used as fallback. In v2.0.1, if `splanet/glide.dll` differs from the embedded GameFrame, the launcher **moves the existing regular file** to the single fixed `glide.dll.bak` (replacing a previous regular backup), installs its verified DLL, and logs both hashes and backup replacement. This prevents backup proliferation; earlier `.bak` contents are intentionally overwritten. Symlinks/non-regular files are rejected. The published v2.0.0 release instead required manual removal of an unknown DLL. `--verify` always stays read-only.

**Validation history:** GameFrame integration was maintainer-tested on Wine-staging, GE-Proton and native Windows 11 (installation, second launch, Alt+Tab, videos and gameplay). The v2.0.0 release candidate was separately exercised on Windows 11 and Wine-staging 11.17; CI checks the native x86 Glide ABI, launcher tests and distribution packaging.

**Logging (v2.0.2 and later):** the launcher appends installation, file-validation, hardware and process diagnostics to `ScorchedPlanetFix_installer.log`. The GameFrame DLL appends Glide/OpenGL diagnostics, menu telemetry and initialization errors to `ScorchedPlanetFix.log`. Both files are beside the launcher. A unique `session_id` is written into both logs for each game launch, even if the DLL fails before rendering. Existing unified `ScorchedPlanetFix.log` contents are not discarded; newer DLL sessions are marked with `SCORCHED_SESSION`. The historical v2.0.1 binary used the earlier unified logging scheme.

**Licenses:** ScorchedPlanetFix GPL-3.0-or-later, bundled GameFrame Apache-2.0. Original commercial game files and proprietary drivers are never included.


## Native Windows 3dfx startup foreground (2026-11-20)

During a successful full FR/DE retail 3dfx GameFrame Windows 11 run, the
maintainer confirmed graphics, sound, controls, Alt-Tab and game behavior
but observed that the game window **initially appeared behind other windows**.
This is not intentional. The shim builds a temporary fallback OpenGL window
before the original `Scorched Planet` HWND exists, then hides that fallback
and adopts the game's HWND on its first available frame.

The Scorched-only pinned GameFrame patch now requests
`SetForegroundWindow(game_hwnd)` **once immediately after fallback hiding**
and emits `SCORCHED_WINDOW: initial foreground request accepted=true|false`
when runtime logging is enabled. The Win32 API can refuse activation under
foreground-lock rules; it is **not forced** and is never retried every frame.
This retains manual focus changes and Alt-Tab behavior, and does not touch
Matrox MSI95 or the game's executable.

Manual regression: launch from a foreground PowerShell/Explorer window and
check that the 3dfx game automatically appears above it; then Alt-Tab away
and confirm it stays in the background until explicitly selected. Check
keyboard/mouse inputs, menu/cinematics, restart, and clean exit.
A successful compilation does not establish that Windows accepted the
activation request, so inspect the new log and visually verify it.


## New Game menu lag: collect phase-specific frame gaps (2026-11-20)

The owner reports that opening the menu to start a new game can take
several seconds, reminiscent of the original fast-CPU fade bug. Two newly
uploaded logs, named `ScorchedPlanetFix(20261120-205441).log` and
`ScorchedPlanetFix(20261120-205522).log`, have **identical SHA-256**
`f3785d742d29ae4defc080abe0d1a9b950102e08a8bc046bf9516ed4dbf19243`;
both actually contain the **earlier #518-era session**
`1795207287255940900-18632`, version `2.0.2-ca502847`.
They cannot be treated as two independent post-#519 runs.

That baseline log contains a menu-probe interval of `9212ms` for
`114` frames just as the main-menu `new` visual score changes,
followed by normal ~59 FPS. This is **consistent with** a delayed menu
transition, but log sampling does not yet reveal whether the stall is
inside the game or in GameFrame. The installed retail FR/DE 1.06 patch
already has the owner-validated **FadeLow four-byte threshold change
(0.0001 → 0.00001)** plus its separate two-byte No-CD gate fix.
Do not blindly patch the same threshold again or loosen the FPS cap
without evidence.

The Scorched-specific GameFrame patch now logs, only with `[Debug] Log=1`,
up to 32 frame delays of >=80 ms, separating:
- `SCORCHED_GAME_FRAME_GAP frame=... outside_wrapper_ms=...`: time since
  the previous `grBufferSwap` returned until the game next called it;
  can include original-game CPU work, movie state, scheduling and waits
- `SCORCHED_GLIDE_FRAME_DELAY frame=... inside_wrapper_ms=...`: time
  spent inside GameFrame `grBufferSwap`, including OpenGL flush,
  configured frame cap, and movie pump

No pacing, timing thresholds, resource loading, original game executable,
FadeLow/no-CD bytes, or Matrox behavior are modified by this telemetry.

**Manual two-run comparison:** with the newest CI build and `Log=1`,
reproduce main menu → New Game at `FPSLimit=60`, capture the real new
session ID and log; next optionally repeat with `FPSLimit=30`, keeping
all other settings identical. Compare actual visual delay, both timing
markers and `SCORCHED_MENU_PROBE` intervals. If the 30 FPS cap changes
the delay, this implicates frame-rate-sensitive sequencing but does not
alone prove the old fade loop is the cause.


## Reproduced 2.72-second New Game block: source isolated (2026-11-20)

The Windows 11 owner reports the New Game submenu takes approximately
three seconds to appear. Unlike earlier duplicate runtime uploads, two
**new, distinct** GameFrame #521 logs identify the precise blocked phase:
- `ScorchedPlanetFix(20261120-210459).log`: session
  `1795208671783303900-8056`, `SCORCHED_GAME_FRAME_GAP frame=171
  outside_wrapper_ms=2721`
- `ScorchedPlanetFix(20261120-210531).log`: session
  `1795208704667705400-24124`, `SCORCHED_GAME_FRAME_GAP frame=153
  outside_wrapper_ms=2727`

**The game does not call `grBufferSwap` during the 2.72s freeze.**
There are no matching `SCORCHED_GLIDE_FRAME_DELAY` reports during this
interval; the only >80ms Glide frame cost in either run is startup
OpenGL setup (258–259ms at frame 2). The change in `new` menu score
follows the gap. A 6ms variation across two runs suggests a repeatable
in-game timed transition; there is no evidence that reducing GameFrame
`FPSLimit=60` or repeating the existing FadeLow patch would help.
The same logs also show the startup focus fix succeeded on Windows:
`SCORCHED_WINDOW: initial foreground request accepted=true` in both.
This confirms the Win32 API accepted the request, while human visual
foreground/Alt-Tab behavior still requires explicit confirmation.

The next optional diagnostic adds `game_thread_cpu_ms=Some(N)` to the
existing `SCORCHED_GAME_FRAME_GAP` line, using the Win32
`GetThreadTimes(GetCurrentThread())` API for the *same game thread*
between consecutive `grBufferSwap` calls. This is passive and enabled
only with `[Debug] Log=1`. Approximately 2.7 seconds of thread CPU
would implicate a busy-loop or CPU-bound game function; near-zero CPU
would suggest `Sleep`, a blocking object wait, I/O, or another thread.
Do **not** conclude which exact instruction is at fault without further
binary-level evidence. `GetThreadTimes` does not stop game threads
or modify any executable instructions.
