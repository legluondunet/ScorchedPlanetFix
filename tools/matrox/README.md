# Matrox Mystique MSI95 clean-room driver (v0.36 experimental)

This directory contains the **independently implemented, open-source**
MSI95 Win95 compatibility layer used by the experimental Matrox OEM backend.
It is not the original Matrox proprietary `msi95.dll`.

The full INI-FPS-enabled C source is directly reviewable in
[`msi95_probe.c`](msi95_probe.c), with SHA-256:

`a439db1acb947646b5fdf7a9f19ef2d7ae542b3c1adeb2c9864c1ae0401194c9`

No opaque precompiled Matrox driver is committed.

The Windows CI executes `tools/matrox/build-msi95.ps1` which checks the
exact source hash and builds a freestanding x86 PE32 DLL
without the original Matrox SDK or proprietary binaries. The nine MSI95
exports and PE architecture are checked by `audit_exports.py`, and the
resulting DLL is embedded with GameFrame in the Windows x64 Rust launcher,
using a build-time SHA-256. The running launcher verifies that hash again
before installing the DLL into the single game/ directory.

This is still a diagnostic-origin renderer: its default logging and
automatic framebuffer BMP capture is disabled; only an explicit F12 manual capture is supported.

## Runtime INI options

The Windows launcher reads `ScorchedPlanetFix.ini` beside the launcher and passes
`[Graphics] FPSLimit` to MSI95 through `MATROX_MSI_FPS_LIMIT`. The
`[matrox] Dither` value is passed separately through `MATROX_MSI_DITHER`.
Both options default to 60 and 0 respectively; `FPSLimit=0` disables
pacing and valid positive caps are 1..240. No separate Matrox CLI option is
needed. The two renderer environment variables are implementation details.

The single Matrox runtime DLL resides at `game/msi95.dll` next to `game/splanetw.exe`. The original proprietary OEM DLL is authenticated during CD extraction and is NOT retained as a backup. GameFrame uses the same `game/` directory when the retail edition is installed instead.

## Debug logging

`[Debug] Log=0` is the new launcher-root default. The launcher passes
`MATROX_MSI_LOG=0`; in this mode `ensure_log` never opens a runtime log,
registers the debug-only exception hook, or prints runtime diagnostics.
`Log=1` enables `ScorchedPlanetFix.log` beside the EXE. The installer
creates/truncates that log once per game session. Installer logs stay
available independently.

## Repeatable F12 diagnostics (2026-11-20)

Each separate F12 press produces a numbered pair of native 640x400
RGB565 BMP files inside `game/`, next to the MSI95 DLL:

- `matrox_capture_0001_frame.bmp`: original framebuffer.
- `matrox_capture_0001_coverage.bmp`: raster ownership map of the same frame.

Next presses create the corresponding `0002`, `0003`, etc. The driver
skips serial numbers if **either** file already exists from another
session (or an interrupted capture). It uses Win32 `CREATE_NEW`
for both files to prevent overwrites, even with a late collision.
Holding F12 cannot produce repeated pictures through key autorepeat:
release and press again.

Automatic BMP capture remains disabled. This does not change
`[Debug] Log=0` or the first-run log policy.

## Crash telemetry from user Wine 11.19 test (2026-11-20)

A verified installed Matrox OEM game using embedded MSI95 DLL hash
`be3ef0d0f016022941188e4dffd0f15bb4c45e83dffc36796fbe720051cdc0ac`
entered a no-geometry scene transition around frame 480 and failed at
frame 498 with `0xC0000005` (read of `0x00000024`) in the game
`splanetw.exe` at `0x00494965`. The crash was not due to an installer
verification failure; it occurred after DLL installation and game launch.

The C wrapper already records the full exception address, register state,
stack, and buffer state when `[Debug] Log=1`. It now additionally records
`AV_CODE before16_and_after16=`, the 16 code bytes preceding and
following the faulting EIP, separated by `|`. This is a strictly read-only
exception probe; it does not intercept/repair/continue the fault, does not
modify the game EXE, and is disabled with normal `Log=0`.

The crash remains **unresolved pending instruction decoding and reproduction
of the exact menu transition**. Changes to F12 capture handling are a possible
regression window, but no `MANUAL_CAPTURE` requests occurred in the crash
log, so no causal attribution has been established.

## Crash investigation: null first argument, caller trace (2026-11-20)

Latest reported Wine run (`ScorchedPlanetFix(20261120-134450).log`) has a repeated
access violation at `splanetw.exe+0x94965`, reading `0x24` from address
`NULL`. The previously added instruction-byte trace establishes:

```asm
0x00494960: push esi
0x00494961: mov esi,[esp+8]   ; first function argument
0x00494965: mov eax,[esi+0x24]  ; crashes because esi=0
```

The stack at fault: `[esp+0]=0x0187F080` (saved ESI),
`[esp+4]=0x004186AA` (caller return address), and
`[esp+8]=0x00000000` (null first argument).
The failure reproduces at frame **1227**, ~42 seconds, and previously at
frame **498**. No F12 capture occurred. **234** texture heap allocations and
**46** recognized frees are logged in the latest run; no allocator failures
or heap-count cap rejections. An animation/frame-dependent workaround is
not justified.

A read-only VEH diagnostic now additionally records `AV_CALLER` and
`AV_CALLER_CODE`: the raw saved ESI, caller return address, candidate
first argument, and 48 bytes centered on the caller return address.
This will identify the call instruction immediately before
`0x004186AA`, helping us determine the actual origin of the null object.
Do not skip the crashing function or synthesize a fake object without
understanding the game logic. Root cause is still unresolved.

## Dynamic texture-heap tracking (2026-11-20)

A repeated-click briefing crash under Wine 11.19 revealed an actual **MSI95 shim
limitation**: `g_heaps[256]` refused the game's valid allocations when 256
texture heaps were simultaneously live. The user runtime log
`ScorchedPlanetFix(20261120-143534).log` contained **41**
`msiAllocTextureHeap REJECT pages=4` lines before a game AV at
`0x00482DEC` (NULL base, attempted read at `0x14`).
The previous log also reached the 256-live ceiling. These refusals are
a plausible *cause* of incomplete game scene structures, not proof of
the root cause of every crash.

The clean-room MSI95 now uses a **geometrically growing VirtualAlloc-backed
pointer table**: starts at 256 slots and grows to 512, 1024, etc. before
allocating the next game texture heap. No artificial count cap or silent
pointer drop remains; only actual allocation failures and invalid page
sizes can return NULL. `msiFreeTextureHeap` removes entries via
swap-delete and `msiExit` releases both tracked heaps and the table.
For debugging, the log reports `MSI_HEAP_TRACKER capacity=...`,
`MSI_HEAP_TRACKER peak_live=...`, and `msiAllocTextureHeap REAL_OOM`
if actual Windows memory allocation fails. The source's MSVC x86 ABI,
nine exports, FPS and display logic remain unchanged.

CI now loads the actual rebuilt x86 MSI95.dll under Windows and allocates
and releases **600** 4-page texture heaps in one process to prove that
capacity growth works beyond the previous hard ceiling. The user still
needs to reproduce the repeated-click briefing scenario under Wine.

## Scene-list teardown crash probe (2026-11-20)

Latest Wine 11.19 user log `ScorchedPlanetFix(20261120-150608).log` reaches
mission gameplay after repeated briefing clicks, records **4152 frames**
(~85.8 seconds), and then faults while exiting through the main menu:
`splanetw.exe` EIP `0x0048302D` (`cmp [ebp+edx*4],edi`);
`EBP=0`, `EDX=3`, invalid read `0x0000000C`.

Crucially the new dynamic MSI95 texture-heap tracker grows from 256
to **512** (no 256 ceiling), logs **548 successful allocations** and
**144 recognized frees**, and has **zero** rejected allocations,
invalid size requests, true OOMs, or unknown frees. `msiExit` was
never reached before the game fault; nonzero live heaps at the time
are not proof of a memory leak.

Original OEM EXE disassembly at `0x00483017..0x00483043` shows that
the *game-owned* list manager is attempting to search its table
at offset `+0x0C` even though the base pointer is NULL, while
its entry count at `+0x1C` is 4. This matches a previous fault
at `0x00482DEC` in a closely related scene-list removal routine.
The likely invariant violation is **nonzero element count with a
NULL element-table pointer**; the origin of that inconsistency is
not yet established.

To investigate without another game EXE trampoline/guard, the MSI95
read-only vectored exception diagnostic now records `AV_SCENE_LIST`:
manager address, element-table pointer, list head, count, capacity,
flags and index for these exact two EXE instructions. `AV_HEAP_TRACKER`
reports live/capacity/peak heap tracking. Each recognized MSI95
free now also records the remaining live count. The old
`AV_CALLER` decoding is restricted to the one function whose
calling convention was disassembled (`0x00494965`), avoiding false
caller inference from scene-list stack data. These probes only
run when `[Debug] Log=1` and never alter exception handling.

**No additional executable patch or exception suppression.**
Repeat normal menu exit under Wine; inspect the scene-list snapshot
to distinguish a stale count, a failed resize, and a teardown order
problem. Do not consider this crash fixed without live verification.

## AV input-context probe (2026-11-20)

When `[Debug] Log=1`, `FIRST_CHANCE_AV` now includes a passive
`AV_INPUT_CONTEXT` snapshot: `endframe_depth`, `guest_callback_depth`,
last forwarded Win32 message, its frame, and age in milliseconds.
This tests whether scene-list faults occur synchronously inside the
preview's forwarding of OEM events from `msiEndFrame`, or later during
game-owned teardown. No input or exception handling behavior is changed.
The age `4294967295` means no forwarded event occurred yet.

## Nested input-message pump correction (2026-11-20)

The user reproduced a briefing crash on Wine-staging 11.19 at
`splanetw.exe:0x0048585B` (`EAX=0`, read `0x44`). The read-only
AV probe showed `endframe_depth=2 guest_callback_depth=3` with the most
recent message `WM_LBUTTONDOWN (0x0201)`. This is direct evidence of
recursive OEM WndProc dispatch from the MSI95 software renderer's own
Win32 message pump. The prior small EXE alignment guard cannot reject
a NULL pointer, but its game-side provenance is not yet explained.

The preview now pumps pending Win32 messages **only in the outermost
`msiEndFrame` and outside an active game WndProc callback**. Nested
frames continue to render without consuming any input messages: queued
events are delivered later from the ordinary outer frame pump, with
original order and event content preserved. Existing early-mouse
initialization checks, F12, focus isolation and teardown remain
unchanged. Diagnostic `NESTED_INPUT_PUMP_DEFERRED` and
`INPUT_PUMP_STATS` messages count deferred nested pumps. The
`AV_INPUT_CONTEXT` read-only diagnostics remain active.

This guards a now-confirmed reentrancy mechanism, not the unknown
underlying game-side NULL-list condition. No new game EXE patch or
exception bypass was introduced. Verify under Wine with fast repeated
clicks in the briefing and normal mission/menu exit, and leave PR #36
unmerged pending validation.

## Game global NULL at splanetw.exe:0x0046D76D (2026-11-20)

An independent Wine-staging 11.19 crash at frame 527, 10.62 s, reads
`[EDX+0x0C]` with `EDX=0` from the original game image, *outside* both
`msiEndFrame` and the OEM WndProc callback. An independently recorded
OEM instruction window shows `mov edx,[0x0050DCD0]` before the fault;
the adjacent global `0x0050DCC0` is updated immediately before the
invalid dereference. Texture tracker live/peak=80/81, no heap OOM or rejected
allocations. Nested input deferral was observed earlier, but no reentrancy
was active at the actual fault; causal origin remains unknown.

With `[Debug] Log=1`, the clean-room MSI95 now records the **raw** global
pointer and counter when the tracked pointer changes, at selected frame
intervals and during game callbacks/texture transitions. At the exact AV
it captures `AV_GAME_GLOBAL_50DCD0` and `AV_GAME_STACK first12`, plus
existing `AV_INPUT_CONTEXT`/`AV_HEAP_TRACKER`. This probe checks the
actual bytes of the OEM `mov edx,[0x0050DCD0]` and `cmp [edx+0x0C],eax`
before enabling itself. It makes no assumption about the global's semantic
type and never modifies original game data, event timing or exceptions.

**Next test:** reproduce the same transition, preserve the new log and
Wine fault. Compare pointer snapshots to learn if it becomes NULL earlier
during a scene change or was already NULL before the fault. Avoid adding
a speculative binary guard or categorizing this as the old scene-manager
`0x0048302D` crash until game control flow is established.


## Offline MSI95 runtime log triage and regression checks

Use `tools/matrox/analyze_runtime_log.py` to review **existing** user-created
`ScorchedPlanetFix.log` files without running the game, reading proprietary
assets or changing gameplay. Python 3 with only the standard library is
required. Run from the repository root:

```text
python tools/matrox/analyze_runtime_log.py /path/to/ScorchedPlanetFix.log
python tools/matrox/analyze_runtime_log.py --json /path/to/first.log /path/to/second.log
python tools/matrox/analyze_runtime_log.py --assert-clean /path/to/ScorchedPlanetFix.log
python tools/matrox/test_analyze_runtime_log.py
```

The `--assert-clean` variant returns nonzero unless all supplied sessions show
both `msiExit` and `SHUTDOWN_COMPLETE`, zero live tracked heaps and **no**
`FIRST_CHANCE_AV` entries. A missing/unreadable log, incomplete session,
post-exception shutdown or uncertain/missing heap count is **not** silently
labelled clean. The output deliberately distinguishes the last *reported*
nested-input counter from an exact final count. It reports observed global
pointer transitions, but does not treat `[0x0050DCD0] == 0` as a failure:
several clean Wine sessions contained normal zero/nonzero transitions.

The test suite uses synthetic log lines only; none of the tester's original
logs or copyrighted game assets are committed. CI runs it automatically on
Windows before the game binary and the two renderers are built. This tool is
a log **classification** aid, **not a fault-reproduction test** or proof that
the earlier intermittent AVs have been eliminated.

### Pre-merge compatibility checklist (Matrox 2.1 draft)

1. Matrox OEM under Wine: verified ISO, normal briefing, several minutes of
   gameplay, return to menu, normal quit. Attach log with `[Debug] Log=1`.
   Also check rapid briefing clicks and verify text is stable visually.
2. Verify that Alt-Tab and return, focus changes, F12 capture and standard
   mouse/keyboard input work without stealing the session or forcing an
   unexpected OEM quit; collect log if they do not.
3. Matrox OEM under native Windows: repeat the same mission/menu/quit flow
   and check the proper preview aspect ratio, text, shadows, textures, audio,
   and responsiveness. The CI input pump smoke can be run interactively on
   Windows to actually test ordered callbacks. A headless GitHub Actions
   `MSI95_INPUT_PUMP_SMOKE_SKIP` **is not** an interactive pass.
4. Retail CD/3dfx GameFrame: install and launch separately, confirm borderless
   fullscreen and HUD, input, mission/menu transitions, cinematics, audio,
   Alt-Tab, normal exit and single-session log policy. These tests are
   essential because the launcher and runtime install/selection code changed.
5. Re-run Windows CI after the last changes. Keep PR #36 as a draft until
   target crashes `0x0046D76D` and `0x0048302D` are understood or a
   deliberately scoped, regression-tested workaround is agreed.

Mark every manual test `PASS`, `FAIL`, or `NOT RUN`; never infer a
particular gameplay navigation path from a renderer log alone.


## CRT-correct 4:3 presentation (2026-11-20)

The Matrox OEM game still creates its original **640x400 RGB565**
framebuffer, an 8:5 *sample grid* historically displayed on a 4:3 CRT
with non-square pixels (source pixel aspect 5:6). The previous preview
mistakenly kept square pixels, displaying 8:5. On an 8:5 Windows desktop
(e.g. scaled 1536x960 logical pixels over a 1920x1200 physical display),
that filled the entire screen and stretched HUD/world horizontally.

The preview now presents the whole 640x400 source as a **4:3 image** in a
centered GDI rectangle; any unused window area is painted black.
Borderless fullscreen is retained. At a 1536x960 *logical* desktop, the
viewport is **1280x960** with **128px sidebars**; at physical 1920x1200,
assuming the same 125% desktop scaling, these correspond to 1600x1200 and
160px sidebars. At 1920x1080 without DPI virtualization, the viewport is
1440x1080 with 240px sidebars. The last values are geometric expectations,
not claims of live visual verification.

Only `preview_blit`'s GDI presentation rectangle and black-bar painting
change. There are **no changes** to framebuffer dimensions, shaders,
textures, geometry, game EXE, Windows focus handling or mouse/keyboard
callback coordinates. A single `PREVIEW_VIEWPORT display_aspect=4:3`
line reports the actual rectangle with `[Debug] Log=1` (or first run).

**Post-build validation required:** Matrox OEM on native Windows and Wine,
check HUD/menus/circles visually (no sideways stretch), pillarbox widths,
click targets near both left and right image edges, Alt-Tab and returning,
normal mission/menu/quit, and no AV. If mouse hit targets are displaced,
fix the guest input-space transformation separately with live evidence; do
not blindly modify game input in the aspect-ratio fix. Retail 3dfx/GameFrame
is unaffected in source but still needs its independent regression test.
