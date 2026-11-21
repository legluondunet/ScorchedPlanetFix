# Matrox Mystique MSI95 clean-room driver (v0.36 experimental)

This directory contains the **independently implemented, open-source**
MSI95 Win95 compatibility layer used by the experimental Matrox OEM backend.
It is not the original Matrox proprietary `msi95.dll`.

The full INI-FPS-enabled C source is directly reviewable in
[`msi95_probe.c`](msi95_probe.c), with SHA-256:

`81396f1f51e2e926d8619dc31d8411e86e67ab24198232692298a9c60b9233d1`

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

## Self-contained runtime OS diagnostics (2026-11-21)

When `[Debug] Log=1` (or the first-run diagnostic override) is enabled,
`ScorchedPlanetFix.log` now records the execution platform at the beginning
of the MSI95 session: `SYS-OS` identifies native Windows versus Wine,
`SYS-WINE` identifies Wine's exported runtime version, `SYS-HOST` identifies
the host OS reported by Wine, and `SYS-PROTON` reports whether known
Steam compatibility **environment hints** are present. A Proton hint is
**not proof** of the exact Proton build; absence of the hint does not
exclude Proton. The standalone C DLL probes Wine directly using
`ntdll.dll!wine_get_version`, without depending on a separate launcher
log. No Steam paths, Wine prefixes, usernames or host identifiers are
written. This is passive Win32 logging only, not a graphics or input change.
With `Log=0`, the host probe is skipped entirely.

## Native Matrox taskbar icon (2026-11-21)

The MSI95 borderless/windowed Win32 preview loads the OEM game's existing
`RT_GROUP_ICON` resource **101** from the already-installed original
`splanetw.exe` (contains 16x16, 32x32 and 48x48 variants). It registers
the 32x32 class icon and sets both small and large window icons with
`WM_SETICON` **before** showing the window, for Windows and Wine/XFCE
taskbars. If the executable's icon group is unavailable, the standard Win32
application icon is the fallback. No icon asset is bundled, the game EXE is
not modified, and the original game window callback does not receive the
renderer-only `WM_SETICON` messages. Debug logs include
`PREVIEW_WINDOW_ICON OEM_group=101 big_present=... small_present=...`.

## EXCLUSIVE experimental A/B build — opaque black index 1 (2026-11-21)

**WARNING:** This stacked PR is not the validated Matrox renderer and is
not intended to be merged into `main` without on-device visual testing.
It is stacked on the separate passive diagnostic PR #39, not on retail
3dfx/GameFrame code.

In the owner's 0010 Difficulty capture, source palette index **1**
resolved to black RGB565, was rejected 20,683 times in the unique last
world-sample map, and coincides spatially with the missing black silhouette
behind three portrait heads. The screenshot supplied by the owner has
solid dark silhouette backgrounds. An offline preview replacing **only**
this index's rejected pixels with opaque black approximated that reference,
with index **241** remaining transparent.

For this **experimental build only**, world 128x128, 8-bit indexed textures
with `Transparency=TRUE`, RGB565 key `0x0000/0xFFFF` and resolved black
retain **source index 1** as opaque RGB565 black instead of skipping it.
All other indexed colors (including index **241**) still use their
original keying behavior. Z-planes and 3dfx are entirely unchanged.
The experiment is enabled automatically for this disposable CI artifact
so no INI edits or environment setup are required.

With `[Debug] Log=1`, `MATROX_INDEX1_OPAQUE_PREVIEW` identifies the
build and `WORLD_KEY_SCREEN index1_opaque_preview=` counts distinct
last-attempt pixels drawn with the override (marked **amber** in the
F12 `*_world.bmp` map). As before, one F12 captures four bitmaps;
`*_frame.bmp` is the actual framebuffer.

Test visually in **Difficulty**, then **Language**, **Quit** and, if stable,
a brief mission. Compare against PR #39's #553 build before integrating
any rules into the supported renderer.

## F12 sprite-atlas provenance diagnostics (2026-11-21)

For the unresolved black rectangles around menu portraits, patchy dark
regions and apparently incomplete menu frames, the game now records **four**
matching, numbered F12 bitmaps in `game/`: `*_frame.bmp`,
`*_coverage.bmp`, and **`*_atlas.bmp`**. The third bitmap is
*diagnostic only*. It tracks the **last attempted sparse Z-plane atlas texel**
at each framebuffer pixel; it is **not** a mask of proven missing game art.
Colors:

- **Green**: texture cache page marked uploaded, nonzero texel
- **Cyan**: marked uploaded, zero texel/index (potentially a valid palette color)
- **Magenta**: unmarked cache page but texel nonzero (direct CPU cache write)
- **Red**: unmarked page and raw zero, discarded by the existing sparse-atlas
  heuristic (could be a legitimate zero palette index; **not proof of absence**)
- **Yellow**: color-key transparency discarded from the large Z-plane atlas
- **Black**: no large Z-plane atlas sample attempted at that pixel

On F12, the standalone MSI95 log emits `ATLAS_CAPTURE` category totals
and `ATLAS_TILE` diagnostics on a 4x4 screen grid. These are generated
from the same rendered frame as the original `*_frame.bmp` and
`*_coverage.bmp`. The overlay is **never shown to the player** or used
as a rasterizer input. There is no automatic screenshot generation; regular
frames do not record per-pixel provenance. No INI option or gameplay
setting changes are needed; enable `[Debug] Log=1` to keep the counters.

**Testing:** capture the same menu in the clean-room renderer and the
original-driver VM for comparison. Send all three F12 BMPs from the
clean-room build plus the runtime log. First inspect the red and yellow
areas against the portrait/corner holes; then decide whether palette-index
residency and keying actually need code changes. Existing pixels and the
3dfx backend are untouched.

## F12 ordinary-world texture provenance (2026-11-21)

The user's five menu triplets were inspected: all atlas BMPs were black,
because those menus use ordinary world triangles (green/cyan/blue coverage)
rather than large sparse Z-plane texture reads. This is **not** evidence of
missing texture uploads.

A fourth BMP, `matrox_capture_N_world.bmp`, now records each framebuffer
pixel's **last attempted ordinary world-texture sample** for the F12 frame.
Color legend:

- Black: no world texture sample attempted
- Green: RGB565 (16bpp) texture drawn
- Olive: 8bpp nonzero palette index drawn
- Orange: 8bpp zero palette index drawn
- Dark purple: RGB565 key rejected a non-indexed (4/16-bit) texel
- Magenta: RGB565 key rejected an **8-bit source index 0**
- Yellow: RGB565 key rejected an **8-bit source index 1**
- Violet: RGB565 key rejected an **8-bit source index 241**
- White: RGB565 key rejected another **8-bit nonzero source index**
- Red: palette-index key comparison discarded the sample
- Blue: optional RGB565 0x00FF fallback discarded the sample
- Cyan: 4bpp indexed world texture drawn

`WORLD_CAPTURE` emits counts for all classes with `[Debug] Log=1`.
After the owner's 2026-11-21 08:16 test showed only RGB565 key skips,
the F12 world map further distinguishes an 8-bit **source index zero**
from a **nonzero index that also maps to RGB565 key color**.
The `rgb565_key` total still includes all categories, and
`rgb565_key_index0`, `rgb565_key_otherindex`, and
`rgb565_key_unindexed` log their precise subdivisions. No transparency
decision, source texture bytes, palette, or screenshot framebuffer changes.

The 2026-11-21 F12 capture 0006 from the owner's Language menu
has **13,292 RGB565-keyed 8-bit samples with nonzero source indices**
in the world map and **zero keyed index-0 pixels**. This does not prove
that the skips are wrong: an intentional transparency index can be
nonzero. The next F12-only diagnostic records a histogram of the
**raw 8-bit indices** of keyed samples in the run log:
`WORLD_KEY_INDEX_HIST` and up to eight `WORLD_KEY_INDEX_TOP` lines.
The per-index log counts **all attempted samples**, including overlapping
triangles, and are not the same as distinct pixels in `*_world.bmp`.
No indexed texture data, palette, color-key comparison or output pixel is
modified by this diagnostic.

The 2026-11-21 09:14 runtime log confirms that **only palette indices
1 and 241** are discarded by RGB565 black keying in captured Difficulty,
Language and Quit menus. Its per-sample histograms count overlapping
triangle attempts. In the new F12 world map, **yellow denotes keyed
index 1**, **violet denotes keyed index 241** and **white denotes other
nonzero keyed indices**, so that the origin of missing-looking artwork
can be compared spatially against the original Matrox-driver VM.
A new `WORLD_KEY_SCREEN` log line counts each color's last-attempted
pixels in the F12 world bitmap, unlike the per-attempt `WORLD_KEY_INDEX_TOP`
histogram. All texture, keying, palette and frame output remain unchanged.

The map is diagnostic **only**: CPU writes, later flat primitives or later
Z-plane draws may cover the recorded texture sample. Neither the world/Z-plane
renderer, its transparency choices, palettes, installed assets, original
binaries nor the 3dfx backend are modified.

For next comparison, collect four matching F12 BMPs for Difficulty and
Language, the same-session runtime log and original-driver screenshots.

## Repeatable F12 diagnostics (2026-11-20)

Each separate F12 press produces a numbered group of four native
640x400 RGB565 BMP files inside `game/`, next to the MSI95 DLL:

- `matrox_capture_0001_frame.bmp`: original framebuffer.
- `matrox_capture_0001_coverage.bmp`: raster ownership map.
- `matrox_capture_0001_atlas.bmp`: last sparse-atlas sample provenance,
  with color legend and limitations documented above.
- `matrox_capture_0001_world.bmp`: ordinary-world texture provenance,
  with color legend and limitations documented above.

Next presses create the corresponding `0002`, `0003`, etc. The driver
skips serial numbers if **any** of the four files already exists from
another session (or an interrupted capture). It uses Win32 `CREATE_NEW`
for all four files to prevent overwrites, even with a late collision.
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


## Narrow OEM scene-transition Space-character guard (2026-11-20)

An *early, rapid double Space* on native Windows 11 after the 4:3 preview
change reproduced game AV `splanetw.exe!0046D76D` at frame 58, 2.734 s
after startup. The source code's original instruction sequence reads a
game-owned pointer from `[0x0050DCD0]` and dereferences `[EDX+0x0c]`.
The game pointer was NULL at the exception; 18 texture heaps were live,
no tracker overflow/OOM. `WM_CHAR(0x20)` was the last guest input event,
15 ms before the AV and shortly after the new scene allocated heaps.
The same crash address had also occurred previously under Wine without
that particular last input, so this is **not a proven universal cause**.

A narrowly scoped **experimental compatibility guard** drops ONLY
`WM_CHAR` carrying a Space character (U+0020), when ALL are true:
the original hash-verified OEM instruction signature matches, its
global `0x0050DCD0` is currently NULL, at least one MSI95 texture heap
is live, the game is initialized, and a successful texture allocation
occurred within the previous 250 ms. Other character messages,
`WM_KEYDOWN/UP`, all mouse messages, focus/Alt-Tab and game graphics
remain unchanged. The guard is active even when `[Debug] Log=0`
(the new `g_last_heap_alloc_tick` state does not depend on logging).
With logging, `TRANSITION_SPACE_CHAR_IGNORED count=... frame=...`
records any intercepted character.

**Do not claim the original engine crash is fixed without live testing.**
Replay the exact rapid-Space startup sequence under Windows using the
new build, verify the new guard log appears (or report that it does not),
verify the game subsequently renders normally, then try normal intro
skipping, menu selection and clean exit. Separately verify Wine and the
retail 3dfx/GameFrame edition: the earlier Wine `0046D76D` crash without
a preceding `WM_CHAR` is not addressed by this guard. No OEM game
instructions or global pointers are modified by this change.


## Native Windows Space-character transition diagnostics (2026-11-20)

The user's previously attached **Windows minidump from 20:18** confirms
`splanetw.exe` faulting at `0x0046D76D`: it loads EDX from the
original game global `0x0050DCD0`, obtains `NULL` and immediately
executes `cmp [edx+0x0C],eax`. The dump itself contains
`[0x0050DCD0]=0`, `EAX=0x11`, `EDX=0`, and an access violation reading
`0xC`. The corresponding runtime log shows the second `WM_KEYDOWN Space`,
a frame-55 texture-heap reload with `GAME_GLOBAL_50DCD0 ptr=0`, then
`WM_CHAR Space` at frame 58 and the crash **15 ms later**.
The OEM game code reaches that dereference through a sequence of MSI
presentation/formatting callbacks. These observations strongly implicate
out-of-phase Space character delivery, but do not prove the underlying
lifecycle flaw or the exact role of the clean-room renderer.

The already-present and edition-signature-locked
`TRANSITION_SPACE_CHAR_IGNORED` compatibility guard suppresses only
`WM_CHAR Space` while the game global is `NULL` and a valid MSI texture
allocation occurred within the preceding 250 ms. It **never** intercepts
`WM_KEYDOWN`, `WM_KEYUP`, other characters, or unrelated scene states.
Its effectiveness has not yet been demonstrated by a matching user-run
suppression trace; a later clean Windows session does not prove it fired.

With **`[Debug] Log=1` only**, the DLL now records the first 12
`WM_CHAR Space` decision points as:

```
TRANSITION_SPACE_CHAR_EVAL frame=... heap_live=... heap_age_ms=...
  scene_global=0x... signature_ok=... callback_depth=... suppress=0|1
```

The existing `TRANSITION_SPACE_CHAR_IGNORED count=... frame=...` line
remains the confirmation that a character was actually suppressed.
A `suppress=0` is a **delivered character**, not evidence of an error
unless it correlates with an AV or unexpected user-visible transition.
The evaluation is capped at 12 messages; successful logging is not a
promise that all gameplay paths were exercised. No runtime game EXE
bytes, media/texture data, FPS pacing, or other input behavior are
modified by this telemetry.

**Next manual Matrox test:** from Windows with `Log=1`, enter the mission
briefing, press Space rapidly twice during the transition as in the prior
crash, then enter gameplay and leave through the menu. Note whether
`TRANSITION_SPACE_CHAR_IGNORED` occurs. Repeating the same transition
without a crash but *without* a suppressed character is useful evidence
of a stable run, not a proof that the guard handles the failing sequence.
Check for both `FIRST_CHANCE_AV` and `SHUTDOWN_COMPLETE`; keep PR #36
in draft until this and the earlier menu-exit scene-list fault are covered.

## Matrox native MSI95 requests versus sparse Z-plane zero samples (issue #48)

The archival MSI95 SDK header documents **no bilinear filter switch**:
https://github.com/kytulendu/MSISDK/blob/master/MSI95/msi95.h.
Restoring original texture/palette and Z-plane behavior is a separate
step from adding modern optional filtering.

**Read-only diagnostics** now count valid OEM `msiSetParameters` requests
in `MSI_NATIVE_REQUEST` (textured transparency, nonzero KeyingAlphaMask,
U/V clamp, depth enabled/protected, color protected) and log the first
16 unusual `MSI_NATIVE_STATE` samples with full keying color/alpha
fields. These counters are **not proof** the renderer accurately implements
each requested mode.

In the existing sparse large-Z-plane-atlas path, `LARGE_Z_ATLAS
zero_skips_color_keyed=N zero_skips_not_keyed=M` subdivides only
the samples **already skipped as unmarked zero**. The first count is zero
values that would have matched the current color key after the palette
lookup (or current Z-plane blue-key), the second is **potentially opaque**
zero values. Neither group establishes whether an unwritten zero was a
real game texel. This counter is *not* the same as
`RENDER_PROGRESS missing_textures`. No pixel write, key, depth rule, or
original game data is changed by this diagnostic.

Use the original 640x400 frame/coverage/world/atlas F12 captures and a
same-scene authentic Mystique VM comparison to determine which skipped
pixels are wrong before changing opacity or adding smooth filtering.
The special-case indexed black portrait fill from PR #40 concerns world
triangles and must not be conflated with large Z-plane atlas residency.

## Opt-in experimental top-left triangle edge rule (issue #48)

The original MSI95 *hardware* edge-fill convention has not yet been
proven. The existing clean-room rasterizer uses an inclusive floating
epsilon for **both** triangles meeting along a shared diagonal. When
pixel centers fall exactly on that diagonal, both triangles can write
those pixels; this may double-apply source opacity or highlights.

For a controlled owner A/B test, `[Matrox] EdgeTopLeft=1` selects the
usual half-open **top-left edge** convention (top and left inclusive,
bottom and right exclusive) for triangles rendered by MSI95. It handles
either vertex winding. `EdgeTopLeft=0` (default) retains the prior
rasterizer's existing inclusion expression **byte-for-byte**.

The helper `msi_edge_top_left.h` is compiled by the runtime AND a
standalone Windows x86 synthetic smoke that executes it on two triangles
covering a 2x2 square, in both orientations, asserting **one writer for
every pixel**, with no holes or duplicate diagonal coverage. This proves
the mathematical behavior of our implementation, **not** that Mystique
used the identical sample convention or that it fixes menu frame edges.
The test does NOT require game assets.

Suggested A/B: screenshot the **same** portrait/menu and HUD frame with
EdgeTopLeft=0 and =1 (same resolution, options and session), using F12,
and visually check diagonal seams and missing edge pixels. Do not merge
based solely on a green synthetic smoke. This setting has **no role**
in texture sampling, black/palette index1 vs index241 keying, Z-plane
atlas residency, bilinear filtering, shaders, or Smacker lifecycle.
