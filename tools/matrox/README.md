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

## Reproducible F12 cross-layer pixel triage (2026-11-23)

The independent Python standard-library-only analyzer
[`analyze_f12_capture.py`](analyze_f12_capture.py) reads the four **same-frame,
same-serial** 16-bit RGB565 / BI_BITFIELDS captures emitted when the user
presses F12: `_frame.bmp`, `_coverage.bmp`, `_atlas.bmp`, `_world.bmp`.
It does **not** run alongside the game or modify renderer code, original
game assets, images or any captured file. All 13 world, 10 coverage and 6
sparse-atlas colors are decoded using the exact source constants in
`msi95_probe.c` (not rounded RGB24). It accepts one optional `(1)` file
suffix after a browser rename but refuses ambiguous duplicate filenames,
truncation, wrong RGB565 masks, different image dimensions or unknown debug
colors. Capture serial naming and dimensions must still be independently
checked if user files were manually renamed.

Example, from a directory holding one unique quartet:

```powershell
python tools/matrox/analyze_f12_capture.py "C:\captures\matrox_capture_0003"
python tools/matrox/analyze_f12_capture.py "C:\captures\matrox_capture_0003" --roi 184,71,455,352 --json
python tools/matrox/test_analyze_f12_capture.py
```

Coordinates are original 640×400 image pixels; ROI is
`x0,y0,x1,y1` **end-exclusive**. JSON includes the current framebuffer's
black/color counts; last MSI write ownership; last attempted world and atlas
provenance; and a critical cross-check for every pixel whose *last attempted*
sparse-atlas sample was an unmarked zero: whether the actual final framebuffer
is colored or black and which world/Z-plane layer last wrote it. No per-frame
buffer writes, texture sampling or keying rules are changed.

**Semantic warning:** An atlas red pixel means the last *attempted* atlas
texel was unmarked zero; it does **not** mean a final missing pixel. A later
world triangle may have drawn a fully colored result there. Likewise coverage
black means *no MSI write*, **not** necessarily a black actual framebuffer
(the game may draw via CPU). The reported
`black_without_final_msi_write` is a possible investigation target, not
proof of missing graphics or incorrect game assets.

**Verified owner capture comparison (raw F12 difficulty menu)**:
`matrox_capture_0003` versus `0004` (EdgeTopLeft=0 vs 1).
Within central panel `ROI=184,71,455,352` (76,151 pixels), the
indexed-1 opaque-preview source positions were **18,760 vs 18,762**.
Both atlas provenance maps had **zero sampled atlas pixels** in that region.
Every **black** framebuffer pixel in the panel (18,474 vs 18,476) had
**a valid final MSI texture write**, not an uncovered/hole marker. Thus no
atlas-Z residency explanation can be inferred for the portraits in those
frames. This corroborates the narrower source-index-1 and source-index-241
palette-key distinction, but not yet exact authentic Matrox transparency
semantics or missing border fragment origins.

Automated tests use **synthetic 2×2 BMP fixtures only**, validate top-down
and bottom-up ordering, clipping/ROI, corrupt/wrong-format files, renamed
suffixes, and exact cross-layer semantics. The CI test step does not need
nor redistribute any copyrighted game data.

## F12-only CLUT color-key collision diagnosis (2026-11-23)

The authentic original MSI95 tutorial (`MSI95/msitutor/msitutor.c`,
`ShowLUT8`) uses 128×128 8-bit indexed textures and two separate
RGB565 CLUT uploads. Its example enables `msiTexture.Transparency`
with `KeyingColor=0` and `KeyingColorMask=0xffff`.
This authentic SDK usage proves that CLUT selection and RGB565 key state
must be recorded **together**. It does NOT prove whether real Mystique
hardware keys indexed pixels before or after palette lookup.

On any explicitly requested F12 frame, this branch logs:

- `F12_PALETTE_BIND`: up to **24 distinct** 8bpp transparent world
  `(texture_offset,lut_offset,key,mask)` combinations and for each the
  CLUT's RGB565 values at source indices 0, 1 and 241. The diagnostic
  also counts how many of all 256 CLUT entries match the current RGB565
  key after applying its mask. This **does not** mean all matching
  indices are present in the texture or sampled in the frame.
- `F12_PALETTE_KEY_CANDIDATES`: counts all real **sample attempts**
  whose resolved RGB565 value matches the active key **before** the
  experimental index-1-opaque bypass. Reports total, distinct sampled
  raw indices, source 0/1/241 and the count of actual 128×128
  preview-sample attempts. These are cumulative **sample attempts,
  not distinct final screen pixels**.
- Existing `WORLD_KEY_INDEX_HIST` counts only **discarded** sample
  attempts (after the preview exclusion). The F12 `_world.bmp`
  remains the **last attempted** indexed source map; neither histogram
  overrides the final-write `_coverage.bmp`.

The source-index-1 opacity exception remains unchanged, the original
RGB565 key predicate remains unchanged, and **no new filter, hook or
rendering modification is enabled**. The extra 256-entry CLUT scan is
gated strictly to an F12 frame, 8-bit transparent world draws and a
bounded 24 **distinct bind** maximum per frame. No additional BMPs or
game-data copies are produced.

### How to interpret

If indices 1 and 241 both resolve to key-colored black under the **same
binding**, but only 1 is experimentally rendered opaque, that shows
a palette alias and precisely quantifies our exception. It does **not**
establish what original MSI95 silicon did with those aliases. True
hardware equivalence still requires aligned original Mystique driver
reference scenes. Avoid making all RGB565-black pixels opaque or blindly
switching every texture to pre-CLUT source-index keying.

## Original OEM `TITLEM.RWB` asset evidence: index 241 belongs to `icon01` (23 November 2026)

An external, private analysis of the actual owner's OEM Matrox ISO
finds the original game file `SPLANET/TITLEM.RWB` (**678,721 bytes**,
SHA-256 `268b99c0c20f9d8ea8cdabb32bd0c6f0446b6f5d5e10de8aac5a3113a21bb408`).
The OEM-specific historical stream has nested `TEXU / STNG / RAST /
STRT / DATA` tags. Its 128x128 raster entries declare 32,768 bytes
of data. Inspecting **only the first 16,384 index bytes** from each
named texture, without guessing the role of the remaining bytes:

| Original atlas name | Raw index 1 | Raw index 241 | Raw index 0 |
|---|---:|---:|---:|
| `icon00` | 5,950 | 0 | 0 |
| `button` | 2,011 | 0 | 0 |
| **`icon01`** | **0** | **6,993** | **0** |
| `FACE3` | 9,040 | 0 | 0 |
| `FACE2` | 8,188 | 0 | 0 |
| `FACE1` | 6,473 | 0 | 0 |
| `FLAGS` | 1,792 | 0 | 0 |
| `FLAGS02` | 0 | 0 | 0 |

*The file contains additional unnamed raster entries; the table covers
eight validated, named 128x128 atlas entries. This is not evidence
that **all** Matrox game assets use index 241 only in `icon01`.*

Within `icon01`, the four 64x64 atlas quadrants contain source
index 241 at counts **1,809 / 2,131 / 2,248 / 805** (top-left,
top-right, bottom-left, bottom-right), respectively. These encode the
joystick, floppy disk, PCB card, and flags icon artwork. The live
F12 world-provenance map in owner Windows screenshot
`matrox_capture_0001` revealed **2,225** keyed-only pixels under
the PCB icon and **805** under the flags icon; the original atlas
counts independently predict the spatial locations. The slight 23
pixel difference on the PCB can result from clipping/rasterization;
this remains a plausible explanation, **not** an independently
proved pixel-by-pixel reconstruction.

Existing live F12 CLUT logging confirms `source[1]` and
`source[241]` both produce RGB565 **`0x0000`**, using a
`KeyingColor=0` / `Mask=0xffff` state. The 3dfx version's
main-menu icon PCB also rests on a near-black/navy background,
corroborating that the icon background is intentional. **However,
the 3dfx screenshot is a different menu state from the Matrox
FACILE dialog**, so the images do not prove exact per-pixel Matrox
Mystique hardware fidelity or the 3dfx RGB color must be reused.

**Engineering consequence:** On the specific original `icon01`
atlas, opacity of index 241 is strongly justified; using a blanket
raw-index-key rule across *all* world indexed textures remains
unproved and would also change index-0 semantics outside this scene.
Prefer an **`icon01`-scoped** exception, identifying the
runtime uploaded texture by a stable original-resource fingerprint
rather than a volatile heap pointer or guessed CLUT offset.
A candidate fingerprint of the first original `icon01` 16,384
index bytes is SHA-256
`cad68634047015739d69bcbd42fb20028c9191bab4b1c3e09a1b51b045bc14b7`
(computed from the actual owner's original ISO; the source atlas must be matched against
the runtime uploaded bytes; do not assume identity blindly).
Do not modify or redistribute the original game atlas, and do not
enable a new rendering rule without testing the full game.

This repository includes a **read-only** standard-library script:
```powershell
python tools/matrox/inspect_titlem_atlases.py "game/TITLEM.RWB"
python tools/matrox/test_inspect_titlem_atlases.py
```
It accepts the owner's original extracted file, emits only atlas names,
index counts and SHA-256 fingerprints (not texture bytes, images, or
game assets). Synthetic CI tests validate parsing and strict header
checking. The optional tool is separate from the launcher/runtime.

## Icon01-only background opacity A/B (23 November 2026)

**Scope and rationale:** A real native Windows F12 session with the previous
`SourceIndexKey128=0` mode identified the actual original title atlas:

```text
F12_TEXTURE_ATLAS frame=234 texture_offset=16205312 lut_offset=16221696 fnv1a32=0x35107CBA raw_index0=0 raw_index1=0 raw_index241=6993 original_titlem=icon01_FINGERPRINT_MATCH
WORLD_KEY_INDEX_TOP rank=1 source_index=241 attempts=7020
```

In the 640x400 *main title menu*, **6,925 distinct final keyed-only
pixels** correspond to this `icon01` atlas: 2,117 in the floppy icon,
1,778 in joystick, 2,225 in PCB and 805 in flags. The original asset
has 6,993 index-241 texels, with quadrant counts 2,131 / 1,809 /
2,248 / 805. 3dfx renders an opaque dark background under the
corresponding art, consistent with the texture's intended appearance,
although **the original Matrox Mystique driver parity is not yet
confirmed**.

**Option, default OFF:**

```ini
[Matrox]
Icon01Opaque=0
```

`Icon01Opaque=1` preserves black RGB565 **only for source index 241**
when all conditions hold:

- world (not Z-plane/HUD) triangle, 8-bit indexed, 128x128 texture;
- transparency is requested with `KeyingColor=0` and
  `KeyingColorMask=0xffff`; palette resolves source index 241 to
  RGB565 0x0000;
- the *current cached texture* is an original TITLEM `icon01` **data
  fingerprint match**: FNV-1a32 `0x35107CBA`, 6,993 source-index-241
  bytes, and zero source-index-0 and source-index-1 bytes.

The hash and counts are computed **only on complete 16,384-byte
texture uploads** (not on every pixel or every triangle). If another
texture is uploaded over the matched cache memory range, the match is
invalidated **before** the next drawing operation. Cache offsets are
used only to locate the matched texture *within that runtime session*:
they are not hardcoded to any value observed in a previous run.

**No other palette index, other 8-bit atlas, 4/16-bit texture, HUD,
sparse Z-plane, retail 3dfx/GameFrame code, game EXE or user game
asset is modified.** The existing black-index-1 preview remains in
place. `Icon01Opaque=0` preserves the previously tested output.

Runtime lines show `ICON01_OPAQUE enabled`,
`ICON01_OPAQUE original_titlem=icon01_FINGERPRINT_MATCH`, matched
uploads, and source-index-241 sample attempts. One F12 outputs
`WORLD_KEY_SCREEN ... icon01_index241_opaque=...` and the teal
world-provenance category `indexed8_icon01_index241_opaque`.
These sample counts may differ from *final* distinct framebuffer pixels
because of overdraw.

The native MSI95 x86 pixel smoke test uses an **artificial 128x128
texture with a deliberately constructed 32-bit FNV collision and matching
source-index counts**. It is **not** the original `icon01` or any
other proprietary game asset. It proves the real renderer recognizes
the selected signature, draws matching index-241 black texels only
with `Icon01Opaque=1`, and **stops** treating a cache offset as
`icon01` when a different texture overwrites it. A signature is
a *compatibility guard*, **not a cryptographic authenticity
certificate**. Invalid/duplicate INI entries are rejected and
existing INI values remain intact.

**Minimal owner test:** Windows or Wine Matrox OEM, enable
`[Matrox] Icon01Opaque=1` and `[Debug] Log=1`, select the
main menu, press F12 **once**. Send the log and four matched BMPs;
there is no need for another mode-0 run because
`matrox_capture_0003` at 2026-11-23 11:16 is the same-scene
baseline. Check all four source-241 icon backgrounds plus the
unchanged index-1 icon backgrounds. Do not enable by default, merge,
or assume exact original-driver RGB values without matching historical
hardware reference.


## Optional modern 3D bilinear texture filtering (2026-11-23; corrected after live owner test)

**Modern opt-in enhancement, NOT original Mystique silicon.** The 1996
Mystique used nearest texture sampling. `WorldBilinear=0` remains the
unchanged default. `[Matrox] WorldBilinear=1` tests smoother world
textures at an additional per-pixel CPU cost.

### Critical correction after actual Windows gameplay (2026-11-23 12:05)

The owner's genuine Windows-native runtime log
`ScorchedPlanetFix(20261123-120532).log` with F12 quartets `0005`
(frame 3390) and `0006` (frame 3591) revealed an unintended
**zero-effect** implementation in the initial PR #55 build:

```text
WORLD_BILINEAR enabled=1 triangles=0
WORLD_BILINEAR filtered_samples=0 changed_pixels=0
PERSPECTIVE_UV enabled=1 triangles=453430
PERSPECTIVE_UV adjusted_pixels=345493542
```

The originally overconservative restriction
`!g_state.msiTexture.Transparency` excluded *every* eligible 3D
terrain texture in this real session, even though the game requests
transparency on these textures and their actual interior texels are
opaque. The original 0005/0006 screenshots therefore remain
nearest-neighbor despite the INI switch; **do not claim they visually
validate bilinear**. This is corrected on the next branch commit.

### Current filtering algorithm and safety

- Only 8-bit indexed (CLUT) or 16-bit RGB565 **world** textured
  triangles with valid **varying inverse-W across vertices**, valid
  original cache binding and full `opacity=100`. No 4-bit palette,
  untextured, Z-plane/HUD/text, constant-invW title icon quad, or
  3dfx/GameFrame path can enter the filter.
- UV interpolation, wrap/clamp, perspective and the original **nearest
  center pixel's key/discard decision** are computed normally first.
- Regardless of whether the game's state requests transparency,
  retrieve the four RGB565 neighboring source samples. Only if **all
  four taps are opaque under the current native RGB565 color-key,
  optional indexed-source key and optional world-blue key** does the
  filter blend their RGB565 channels with 8-bit fixed-point fractions.
- **If any neighboring tap is color-keyed or transparency-discarded**,
  do NOT blend it and preserve the original center nearest color.
  Conservatively treat original index1 and icon01 black exemptions
  as keys for the neighbor test, so silhouettes and icon edges cannot
  acquire colored halos. Existing index1/icon01 center-pixel behavior
  is unchanged.
- No original EXE or textures are patched or copied. Color modulation
  continues after filtering, as before. The existing 640x400 HUD, sparse
  1024x1024 Z-plane atlas, icon01 title correction and native 3dfx
  renderer all remain isolated.

The runtime logs cumulative overdraw-inclusive stats:

```text
WORLD_BILINEAR enabled=1 triangles=N
WORLD_BILINEAR filtered_samples=N changed_pixels=N
WORLD_BILINEAR keyed_neighbor_bypass=N
```

`keyed_neighbor_bypass` quantifies safe nearest-neighbor fallbacks on
would-be filterable 3D triangle pixels, not a count of missing art.

Native Windows DLL smoke uses **18** original icon01/palette pixel
cases plus **32** bilinear cases: filter 0/1 × 8/16 bpp × game
transparency on/off × constant/varying invW × keyed-neighbor absent/
present. The transparent-keyed neighbor must always force the
reference nearest pixel, while opaque neighborhoods smooth even
when the game's transparency flag is enabled. Zero copyrighted
original texture assets.

**Next user test:** one F12 in a playable terrain scene on this
corrected experimental compilation (`WorldBilinear=1`,
`Icon01Opaque=1`, `[Debug] Log=1`). The previous 0005/0006
screens already provide useful *nearest* visual references but are
not the same camera position as a prospective capture. The immediate
acceptance check is that `WORLD_BILINEAR filtered_samples > 0` and
`changed_pixels > 0` with sharp cockpit/HUD; only compare exact
pixel differences if captured at the same game frame and viewpoint.
Performance may be noticeably slower due to 4 texel reads per sample;
return to `WorldBilinear=0` to recover original 1996 nearest look.

### CPU optimization after native Windows #0007 validation (2026-11-23)

The owner confirmed on native Windows that the corrected bilinear filter is
**actually active** in playable 3D: `WorldBilinear=1`,
445,661 eligible world triangles, **363,544,261 filtered samples**,
**209,190,528** changed RGB565 sample colors, and **16,436**
key-neighbor nearest fallbacks in the last periodic diagnostic snapshot.
The HUD and cockpit Z-plane remain in their own native draw path.
The session took 18.88 ms of renderer time per frame on average,
versus 12.25 ms in an earlier *different* 3D session in which the
filter had been a no-op. This suggests a possible significant CPU
cost, **not a controlled same-scene speed measurement**.

The optimized draft branch now improves only the inner filter hot path:

- For an eligible **8-bit indexed world** triangle with projected
  bounding rectangle at least **512** pixels, predecode its currently
  bound, original **256-entry RGB565 CLUT** and a parallel 256-entry
  *original key-aware opacity* table **once per triangle**, using the
  current transparency flag, key/mask, optional source-index key and
  optional blue-key fallback.
- For every shaded sample, read four raw indices and use the two local
  tables rather than decoding four RGB565 palette entries and
  reevaluating four key predicates repeatedly.
- For smaller triangles (and for all 16-bit RGB565 textures), retain
  the previous directly decoded/color-key-checked path. This avoids
  making tiny polygons pay for a full 256-entry CLUT predecode.
- When the four opaque neighboring RGB565 colors are identical,
  preserve the exact same result without doing twelve channel
  multiplications. The `filtered_samples` and `changed_pixels`
  counters retain their former meaning.
- **No cross-triangle / global palette cache**: every eligible large
  triangle re-reads the current palette. In particular, another
  texture/CLUT upload or an original direct CLUT change cannot leave
  stale cache entries affecting later triangles.
- The **original nearest center key/discard check happens first**, and
  every potential bilinear neighbor must still be opaque. The
  existing `icon01`, original portrait index1 and cockpit/HUD
  boundaries are unchanged; `WorldBilinear=0` remains default.

Additional log counters:

```text
WORLD_BILINEAR palette_cached_triangles=N palette_cached_samples=N
WORLD_BILINEAR uniform_neighborhood_samples=N
```

The expanded Windows MSI95 x86 **actual-DLL** regression exercises
**18** existing palette/icon01 cases and **128** bilinear render cases:
`WorldBilinear 0/1` × 8/16bpp × transparency off/on × varying/
constant inverse-W × keyed-neighbor on/off × small/large geometry ×
uniform/nonuniform source texel colors.
Small triangles exercise the uncached path; large indexed triangles
exercise predecoded local CLUT tables. Tests use only synthetic red,
blue and keyed RGB565 texels, not original copyrighted textures.

**Performance remains to be verified in actual gameplay** before
claiming a measured FPS gain or merging. The correct minimal next step
is a single playable 3D scene with `WorldBilinear=1` and `Log=1`,
a F12 capture and the session timing summary. If the game is still
too CPU-bound, do not enable bilinear by default; the historic Mystique
nearest mode is always available.

## Experimental Win32 vertex-region validation fast path (performance)

The MSI95 software backend receives three separately supplied original-game
pointers for every world triangle. Their records must be checked before any
dereference: valid geometry can be transient around OEM scene reloads.

The experimental optimization in PR #61 uses **one fresh** `VirtualQuery`
for the first vertex and reuses its committed, accessible **same memory region**
for each additional vertex *only when the entire vertex struct fits*. If a
vertex lies in another region, or spans a memory boundary, the existing
individual `readable_span` validation still runs. Guard/no-access/uncommitted
memory remains rejected. No reuse between triangles or frames; no permission
changes or original-game memory patches.

The periodic `VERTEX_REGION_CHECK reused_queries=... fallback_queries=...`
line estimates how many Windows memory queries are avoided, not measured CPU
speedup. The real x86 DLL smoke tests cover contiguous stack vertices, three
distinct `VirtualAlloc` regions, low/NULL pointers and a record crossing a
`PAGE_NOACCESS` boundary. Original per-pixel rasterizer, CLUT, Z-plane,
bilinear, dithering, Smacker, game files and defaults remain unchanged.

**Assessment:** potentially useful for triangle-heavy scenes (the owner's A/B
logged over 350k triangles in 900 frames without bilinear), but actual gains
must be confirmed on Windows with matching scenes. This is not a cure for
missing Matrox image regions, transparency, slow per-pixel CPU rasterization or
intermittent original Smacker faults. Keep this PR draft until runtime tested.

## F12 spatial-region triage (experimental offline diagnostic, PR #64)

The existing `analyze_f12_capture.py` now supports **`--regions`** to
identify connected groups of *candidate* problem pixels in a same-serial F12
quartet. It does **not** modify the game, MSI95.dll, textures, keying,
framebuffer, screenshots, or reference images. This extension adds an optional
report alongside the unchanged aggregate counts.

```text
python tools/matrox/analyze_f12_capture.py "captures/matrox_capture_0003" --regions --json
python tools/matrox/analyze_f12_capture.py "captures/matrox_capture_0003" --roi 184,71,455,352 --regions --min-region-pixels 8 --max-regions 8 --json
```

Each class reports the **number of candidate pixels inside the ROI** and the
largest four-connected clusters with area and end-exclusive native 640×400
bounding boxes. Four-connected intentionally keeps diagonal-only touches
separate. Output is bounded to at most 32 regions per category. Categories
are intersections of raw framebuffer color, last *MSI write* and last
*attempted* world/atlas source, rather than just a black-pixel threshold:

- `visible_world_index1_black_preview`: an actual index1 opaque **black
  world write** (known portrait silhouette, not a missing image)
- `world_index241_keyed_only`, `world_index1_keyed_only`,
  `world_other_rgb_keyed_only`: last world sample of a source-index
  category was discarded by a black RGB565 key, and no later MSI write
- `zplane_unmarked_zero_black_unowned`: last attempt sampled unmapped zero
  in a large Z-plane atlas, screen pixel is black, no later MSI write
- `black_without_final_msi_write`: black pixel with no final MSI write,
  which may simply be intentional background

**Crucial:** none of these classifications proves a missing graphic. The
game may write directly to the framebuffer, a last attempted sample may come
from a different draw from the last visible pixel, and an intended
transparent region might legitimately reveal black. `--regions` is a
*triage aid* to find spatial evidence against a matching original Mystique
screenshot; no automated art restoration or source-index key change is
authorized by these clusters alone.

Run on the **already captured** owner F12 quartets first. Existing original
TITLEM.RWB `icon01` fingerprint and same-scene index241 A/B analysis
remain the strongest confirmed title-asset attribution; the larger
1024×1024 Z-plane atlas has previously been ruled out for the portrait
menu scene. No new user screenshot is needed just to exercise the tool.

## Comparison-crop geometry: 9.5% reference zoom (2026-11-23)

This is an **offline** reanalysis of the owner's *previously uploaded*
`Matrox_premier_correctif_comparaison.png` montage (three labeled
640x400 crops: "Avant #553", "Après #554", "Capture de référence") and
the owner's later real F12 `matrox_capture_0001_frame(3).bmp` difficulty
menu. No screenshots or game artwork are committed.

SIFT features + robust RANSAC affine *similarity* alignment reveal that
the right-hand **reference** crop is approximately **1.0952× zoomed**
relative to the middle **after #554** crop. Translation in reference
crop coordinates is about **(+16.04,+0.41) px**, rotation about
**−0.04°**. **441 of 459** feature matches agree with this transform
for the later F12-vs-reference comparison. Comparing the after-#554
montage crop to the later F12 frame at the same native coordinates
gives only **917 / 256,000 different screenshot pixels**, due largely
to pointer movement. That F12 frame is a valid candidate for
correlating the montage to pixel ownership diagnostics.

After resampling the reference into the same 640x400 candidate geometry,
mean absolute per-channel image difference is **3.138 levels** over
**208,050 mutually covered pixels** (8-bit BGR; scaling/interpolation
and cursor motion included). **2.952%** of overlap pixels have any
channel difference above 40. This is **not a native RGB565 bit-identical
claim**. It is evidence that interpreting panel/frame dimensions from the
unregistered side-by-side montage as missing renderer geometry was unsafe.

For the rectangular central panel perimeter
`[184,63,455,336)`, **12-pixel-wide border bands** have these
post-registration any-channel>40 difference rates:
top **3.659%**, left **1.740%**, right **0.201%**,
bottom **0.953%**. The corresponding original F12 coverage categories:
- top: **2,213** flat colored world pixels, **768** textured colored;
  **271** pixels with no final MSI write but **all visibly colored**;
- left: **2,988 final MSI writes**, no last-key-only pixels;
- right: **2,988 final MSI writes**, no last-key-only pixels;
- bottom: **3,121** final MSI writes, **131** no-MSI-write pixels,
  but **all colored**.

There are **zero black+no-MSI-write pixel holes**, **zero
key-discard-only pixels**, and **zero Z-plane final writes** in all
four bands. This **strongly weakens** a missing-triangle, rejected
key, or sparse-atlas explanation for the central panel's outer
borders **in this particular captured scene**. It does **not**
prove that every other UI frame or 3D game scene matches native
Mystique hardware; the screenshot reference may have been scaled by
capture/presentation processing.

The optional tool `align_reference_captures.py` is provided to
prevent misleading comparisons from differently scaled screenshots.
It requires `opencv-python-headless` and `numpy` **only for offline
diagnostics** (neither is shipped to the game runtime):

```text
python -m pip install opencv-python-headless numpy
python tools/matrox/align_reference_captures.py candidate.png comparison.png --reference-crop 1334,52,1974,452 --panel 184,63,455,336 --border-width 12 --json --overlay alignment.png
python tools/matrox/test_align_reference_captures.py
```

The analyzer rejects featureless/dissimilar scenes and implausible
scales/rotation, reports geometric evidence and border-band differences,
and can emit a local comparison montage. It does not claim to recognize
the original hardware, does not treat all black pixels as missing, and
does not apply any correction to the game. Its five CI fixtures are
synthetic; no owner's images are used in CI.

**Disposition:** Do not change center panel raster edge rules on this
montage alone. Instead, find an independently verified same-scene
Mystique-vs-current capture showing a **specific, still discrepant
border segment** and compare it after geometry normalization.

## Cross-scene F12 HUD/Z-plane ownership (2026-11-23)

The owner's **three actual 640x400 gameplay quartets**, `matrox_capture_0005`,
`0006`, and `0007`, come from different camera directions and (for 0007)
an active modern optional world filter. These are **NOT** same-camera,
same-frame 3D image comparisons. However, original MSI95's screen-aligned
Z-plane HUD can be checked separately: native F12 `_coverage.bmp`
category 5 or 6 means the last MSI write in that pixel was a Z-plane.

**Direct audit of original user-provided BMP bytes:**

| Sample | Final Z-plane/HUD pixels | Final Z-plane mask vs 0005 | RGB565 changes within shared HUD |
| --- | ---: | ---: | ---: |
| 0005, nearest | 122,948 | reference | — |
| 0006, nearest | 122,948 | **0 changed coordinates** | **88 / 122,948** |
| 0007, active world bilinear | 122,948 | **0 changed coordinates** | **82 / 122,948** |

All three maps also classify **101,367** last-attempt *unmarked
nonzero atlas* pixels and **24,801** last-attempt unmarked-zero skips,
including **8,586** skipped-zero last attempts **over pixels which
ultimately have an actual final Z-plane write**. Last-attempt zero
does not mean that the final HUD was erased: overlapping later and
earlier Z-plane draws can cover the pixel. The current F12 maps record
no last MSI `hud_missing_texel` ownership in these three frames.

**Practical conclusion:** The *last-write HUD ownership mask is fully
stable* across three **distinct camera views**; the active world-only
bilinear does not cause observed HUD geometry or alpha coverage changes
in these captures. This does **NOT** prove authentic original Matrox
fidelity, rule out missing elements in other gameplay scenes, or
establish that the RGB565 pixels match a vintage Mystique VM reference.
Only same-geometry reference pictures and correct GUI/HUD game state
can answer that.

The new offline `compare_hud_captures.py` makes the comparison
repeatable on any two or more same-serial F12 quartets, **including
different world-camera positions**, without treating changed world
colors as missing HUD pixels:

```text
python tools/matrox/compare_hud_captures.py captures/matrox_capture_0005 captures/matrox_capture_0006 captures/matrox_capture_0007 --json
python tools/matrox/compare_hud_captures.py captures/matrox_capture_0005 captures/matrox_capture_0007 --roi 218,250,420,400
python tools/matrox/test_compare_hud_captures.py
```

It counts final HUD ownership, black HUD, last-attempt atlas zero over
already written HUD versus zero without final HUD write, and
key/missing-texel-only classes. It reports mask symmetric differences
and *exact* RGB565 differences restricted to the overlapping final
HUD mask. It validates exact quartet dimensions and rejects duplicate
source prefixes, invalid RGB565 diagnostic colors and invalid ROIs.
The source uses only the Python standard library and the existing
`analyze_f12_capture.py`; synthetic CI tests require no 1996 assets.

**No game DLL, EXE, INI, texture, cache, filter or frame output is
modified.** This is evidence triage, not a runtime fix. Never assume
a screenshot showing a smaller/different HUD is a renderer bug unless
its original game's view mode and HUD state are also confirmed.

## F12 last MSI shadow ownership — isolated experiment (2026-11-23)

The owner's **22-minute** Wine 11.19 stress log `ScorchedPlanetFix(20261123-190700).log`
shows a notable split at **frame 59,400**:

- `SHADOW_UNDERLAY candidates_with_texture=3450362054`
- `SHADOW_UNDERLAY no_texture=3401080838`
- `SHADOW_ONCE repeated_samples=15359434`
- `SHADOW_BLEND dark_flat_pixels=2571835030` (**one UINT32 rollover**).

The exact accounting identity is:

```text
3,450,362,054 + 3,401,080,838 + 15,359,434
 = 6,866,802,326
 = 2,571,835,030 + 4,294,967,296
```

These are **overdraw-inclusive dark-flat sample attempts**, not
3.4 billion visually missing pixels. They confirm that the existing
`ShadowOnce=1` rasterizer often encounters the original
`RGB565=0x0841` flat-dark triangles with **no previously recorded
textured world fragment at that pixel**. The existing code draws such
candidates **opaque**. This might explain some dark stains, or it might
be original intended untextured shading, sky/background drawing, or
world-order state. A source sample alone cannot settle it.

### One extra, read-only, per-F12 capture

The experimental MSI95 DLL on [draft PR #65](https://github.com/legluondunet/ScorchedPlanetFix/pull/65)
writes one fifth BMP, alongside the original same-serial quartet:
`matrox_capture_XXXX_shadow.bmp` (640×400, 16-bit RGB565 BI_BITFIELDS).
The previous four BMPs remain byte-compatible. The fifth image tracks
**ONLY THE LAST SUCCESSFUL MSI WRITE**, not attempted or repeated
skipped shadows:

| Color | Last-write category |
| --- | --- |
| Black | Other final MSI draw; or no successful MSI draw |
| Green | Dark-flat shadow blended to 50% over a recorded textured world base |
| Red | Dark-flat shadow drawn opaque with no recorded textured world base |
| Yellow | Other dark-flat draw with recorded textured base |
| Cyan | Other dark-flat draw with no recorded textured base |
| Magenta | Unusual prior-blend state |

A later world texture, flat non-shadow, or Z-plane overwrites the
shadow-origin tag, exactly as it overwrites the F12 owner map. Repeated
skipped dark-flat samples do not overwrite the earlier tag. A
`SHADOW_CAPTURE OK file=… blended_with_textured_base=…
opaque_no_textured_base=…` line confirms the fifth bitmap was emitted.

**No shadow/blend/color/key behavior changes.** All shadow metadata is
allocated optionally (256 KiB at 640×400), zeroed and written only for
the one frame explicitly requested by F12. Normal gameplay samples
have no instrumentation writes.

For the new offline cross-layer analyzer (Python standard library only):

```text
python tools/matrox/analyze_shadow_capture.py captures/matrox_capture_0008 --json
python tools/matrox/analyze_shadow_capture.py captures/matrox_capture_0008 --roi 100,80,540,260 --min-region-pixels 12
python tools/matrox/test_analyze_shadow_capture.py
```

The tool validates all **five same-serial bitmaps**, color constants,
screen size, ROI, and shadow-vs-coverage ownership consistency; lists
4-connected native-pixel regions for shadow-with/without-underlay; and
refuses a legacy four-layer screenshot rather than fabricating values.
The Windows CI uses **synthetic** RGB565 fixtures only, no game assets.

**Interpretation guard:** A red pixel means the last world MSI draw
was a dark-flat `0x0841` candidate with no prior *recorded textured*
underlay, **not** that the pixel is incorrect. Original software CPU
writes or deliberately untextured background geometry remain possible.
Only an authentic same-scene Mystique-driver reference can justify
changing this renderer rule. Original 1996 nearest sampling stays the
default, no extra postfilter is introduced.

## 2026-11-24 — Distinguish **final visible world black** by actual source

The owner's real F12 #0005 (Wine 11.19, frame 1706, captured 2026-11-23)
had **2,471 RGB565-black final textured-world pixels** (and **1,614**
nonblack final shadow pixels, disjoint from black). The previous fifth
`_shadow.bmp` ruled out the *opaque-no-textured-underlay* shadow theory
for this scene. But `_world.bmp` is **last attempted world sample**,
including samples later overwritten by CPU/HUD/world draws; its broad
8bpp class cannot attribute a specific black final pixel to the original
palette index. Moreover `F12_PALETTE_KEY_CANDIDATES` counted **3,817**
black-key candidate *attempts* of source index 1, not final pixels.

This new strictly **diagnostic-only** sixth same-serial BMP
`matrox_capture_XXXX_texel.bmp` captures the **last successful MSI
writer's source class per pixel**, reset each selected F12 frame and
overwritten whenever a later MSI draw succeeds (including HUD and flat
shadow). It identifies final black RGB565 by 8bpp original source
index `1`, `241`, `0`, another indexed source, 16bpp texels, or
4bpp texels. Separate classes retain nonblack 8bpp/index1/index241,
16bpp and 4bpp. Source classification is updated **only after successful
raster writes**; any key-discard, blue-key skip, absent texture or prior
overdraw has no final ownership.

```text
python tools/matrox/analyze_final_texel_capture.py game/matrox_capture_0006 --json
python tools/matrox/test_analyze_final_texel_capture.py
```

The analyzer checks per-pixel invariants against real `_frame.bmp`
and `_coverage.bmp`: every final world textured-black pixel must have
a black source class, and the provenance source's *final* black flag
must match the RGB565 framebuffer. ROI is half-open native pixels and
connected components locate each source. It refuses missing or
different-size layers. Synthetic Windows CI validates all eight cases.

**Limitations:** This tracks final *MSI rasterizer* writes, not later
uninstrumented game **CPU framebuffer writes**. It records the source
index even if color is later modulated or blended, so categories describe
the **actual final RGB565 output color**, not necessarily raw source
palette RGB. No raw original Matrox driver evidence exists yet to claim
source index 1 should be keyed or drawn. **No changed keys, palettes,
filter defaults, shadow blending, geometry or graphics behavior** in
this branch. No user/game screenshots or textures committed.

## 2026-11-24 — Source of black pixels: palette RGB565 or post-lighting?

The owner's **real** six-layer F12 #0006 (Wine 11.19, frame 2449)
contained **4,266 final black RGB565 world-texture pixels**:
4,016 from 8bpp palette indices **other than 0/1/241**,
240 from index 1, and 10 from index 241. The fifth shadow layer showed
12,575 **nonblack** final shadows and **no** final opaque/no-texture-base
shadow. Thus globally forcing index 1 keyed or opaque will not fix the
dominant dark ridge. The six-layer F12 map, however, only records whether
the **post-lighting** pixel is black. It cannot tell whether the CLUT
returned RGB565 zero **before** optional MSI modulation and dither.

This follow-up adds a **seventh** same-serial F12-only bitmap
`matrox_capture_XXXX_sourceaudit.bmp` (16-bit RGB565 BI_BITFIELDS).
**It is a lossless 16-bit encoded data map, not an image to interpret by
eye or a screenshot; its color values are intentionally packed flags.**
All previous six captures remain identical.

Packed data per final successful MSI world-texture write:

| Bit(s) | Meaning |
| --- | --- |
| 14 | Final texture source is **8bpp**; low 8 bits preserve **exact source index 0–255** |
| 13 | Final texture source is native **16bpp** |
| 12 | Final texture source is **4bpp** |
| 8 | Pre-lighting sampled RGB565 was **0x0000** |
| 9 | MSI texture RGB modulation was applied |
| 10 | Ordered dither was applied with modulation |
| 11 | Source color was subsequently alpha-blended with prior framebuffer |
| 0–7 | Exact raw source index when bit 14 is set; zero for 16/4bpp |
| 15 | Reserved and always zero |
| entire pixel zero | No final successful ordinary MSI world-texture write |

The three mutually exclusive source-format bits never overlap. Records
are written **after** a successful MSI framebuffer write, and reset by
later successful world/flat/HUD writes. Key-discarded samples **do not**
claim final ownership, regardless of their color or palette index.

```sh
python tools/matrox/analyze_source_audit_capture.py game/matrox_capture_0007
python tools/matrox/analyze_source_audit_capture.py game/matrox_capture_0007 --json
python tools/matrox/test_analyze_source_audit_capture.py
```

The offline stdlib analyzer validates the seventh map against the existing
`_frame`, `_coverage` and `_texel` F12 layers; it **rejects** invalid
flags, dimensions, unsupported palette-source combinations, missing
metadata, and dither-without-modulation. It reports precise **top 8bpp
indices among final black pixels** and distinguishes:

- Original/pre-lighting texture **already black**, remains black
- Texture **nonblack before lighting**, final result black
- Texture black before lighting but output nonblack (e.g. alpha)
- How many final black pixels had modulation, ordered dither, or alpha
- Spatial 4-connected components for source-black vs darkened-to-black

**Interpretation limits:** The sampled pre-lighting RGB565 equals the
CLUT-resolved original color in authentic **nearest-neighbor** sampling.
Optional `WorldBilinear` changes the sampled RGB565 before this capture.
Ordered dithering and alpha can affect black independently of underlying
palette contents; the flags expose these cases. CPU framebuffer writes
after MSI are not intercepted. Source index/CLUT black alone **does not
establish a Mystique-driver rendering bug** without same-view hardware
comparison. This PR changes no keys, colors, modulation, dithering,
shadows, raster algorithms, settings, or copyrighted game assets.
Eleven entirely synthetic tests cover format flags, aliases, overdraw,
black-source conservation and error rejection in Windows CI.

### Verified on owner's real F12 #0007 (Wine 11.19)

The seven native RGB565 BMPs captured on 2026-11-24 at **frame 2952** are
self-consistent. At the last successful MSI write in this terrain scene:
**116,290** world textured pixels = 113,048 nonblack + **3,242 black**;
16,762 nonblack final flat shadows; 122,948 HUD pixels; zero unwritten
pixels or opaque shadow/no-texture final writes. Of the 3,242 final world
textured RGB565-black pixels, **3,015** came from other 8bpp indices,
**225** from source index 1, **2** from 241. Every one of the
3,242 had a **nonzero RGB565 sample before lighting** and both modulation
and ordered dithering enabled; none had alpha blending. Thus neither
intrinsically black palette texels nor an opaque shadow are the direct
source of visible black in this particular image.

The 4x4 ordered Bayer threshold provides useful additional evidence.
Across the 16 ranks, about 5,500 final modulated-and-dithered world
pixels per rank were processed, while final-black incidence rose
monotonically from **20/5,504 (0.36%)** at rank 0 to **374/5,529
(6.76%)** at rank 15. Quartile ranks 0–3, 4–7, 8–11, 12–15 had
**227, 686, 1,001, 1,328** final black pixels, respectively, with
approximately equal denominators. The offline analyzer now calculates
those rank denominators and final black rates automatically from the
existing seventh F12 layer, in addition to source-index/lighting
counters; **no eighth BMP or new owner capture is required**.

**Caution:** The F12 metadata says dithering was *enabled*, not
whether each output color differs from what an undithered reference
would produce. The strong ordered-rank correlation implicates the
4x4 rounding threshold as a contributor, but cannot establish the
Mystique silicon's actual threshold matrix (or prove a regression)
without same-scene hardware reference. Do not silently disable
native MSI modulation or alter original dither/filter settings.
Real user captures remain private, not committed.

## 2026-11-24 — What part of final black is CAUSED by ordered dithering?

The owner's real F12 #0007 demonstrated **3,242 final black 3D pixels**
that were *all nonblack before modulation* and *all modulated and ordered
dithered*. The monotone 4x4 Bayer incidence (0.36% rank 0 to 6.76% rank 15)
implicates the dither thresholds, but **does not prove which pixels would
have remained black with the same modulation and no dithering**.

This strict, passive extension captures an **eighth** same-serial F12 BMP
`matrox_capture_XXXX_nodither.bmp`. It contains the code's EXISTING,
undithered RGB565 modulation reference for each **last successful
modulated-and-dithered MSI world-texture write**. For opacity below 100%,
it uses the **same actual framebuffer underlay** as the game, including
the same integer source-over blending and rounding. It does **not**
alter the actual output color. Subsequent world, HUD or flat/shadow
writes clear the reference: only final successful world texture pixels
are counted. It is RGB565 as a 16-bit BI_BITFIELDS BMP, like the other
F12 layers. All eight numbered outputs share a serial, are written
`CREATE_NEW`, and will never overwrite prior captures.

**Critical interpretation:** The eighth bitmap is a sparse **data map**,
NOT a full no-dither screenshot! Its RGB565 value `0x0000` can mean
either a valid **black** undithered reference or an untracked last
write. Use **bit 10** of the paired seventh
`_sourceaudit.bmp` to identify valid reference pixels. The exact
counterfactual is *only the final MSI write with dithering replaced by
the already computed undithered modulation color*, keeping the actual
prior framebuffer (including any earlier dithered overdraw). It is
NOT a full-frame no-dither replay. Alpha blends are classified
separately so a full-scene causal conclusion is not exaggerated.

```sh
python tools/matrox/analyze_undithered_capture.py game/matrox_capture_0008
python tools/matrox/analyze_undithered_capture.py game/matrox_capture_0008 --json
python tools/matrox/test_analyze_undithered_capture.py
```

The analyzer first verifies the seventh source audit, sixth texel-source
layer, raw frame and coverage consistency. It then distinguishes:

- Final RGB565 black **and would remain black** without ordered
  dithering of this last write (modulation alone enough for black)
- Final RGB565 black but **undithered reference was nonblack**
  (dithering **created** black on the last write)
- Final RGB565 nonblack but undithered reference was black
  (dithering **prevented** black on the last write)
- Other changed vs identical nonblack colors, 4×4 Bayer rank incidence,
  source-index histogram, alpha-use, and four-connected spatial regions.

An independent **eight synthetic tests** cover black creation,
prevention, unchanged dark, alpha, overdraw/HUD clearing, ROI,
collision/size errors and non-dither final writes. The native renderer
remains byte-for-byte unchanged for game pixels, palette/keying,
modulation, dither, shadows and filters; the additional **2 bytes per
pixel** (~512 KiB at 640×400) are F12-only diagnostic metadata.

Even a large last-write dither effect is **not evidence** that the
real 1996 Mystique uses different rounding thresholds: only a
camera-matched native Matrox-driver image can establish pixel parity.
No game assets or user screenshots are committed.

## 2026-11-24 — Exact per-pixel MSI95 texture lighting input audit

The owner's native Windows #0008 and #0009 eight-layer F12 screenshots
established that 2,625 / 558 black textured pixels respectively remain
black **even with the last-write ordered dithering removed**, whereas
454 / 470 final black pixels arise specifically from last-write dither.
The large black terrain patches are therefore primarily connected to
**modulation/quantization**, not a bad chroma-key or the 4x4 dither alone.

The missing evidence is the precise **per-pixel original 8-bit Gouraud
coefficients** and source RGB565 that this experimental software renderer
multiplies. Do not change render brightness until we can identify whether
values are genuinely small or the CPU implementation is inconsistent.

This follow-up records a **ninth** same-serial, F12-only diagnostic
`matrox_capture_XXXX_lighting.bin`. This is intentionally a compact
**binary data stream**, not a screenshot/texture image. It contains no
original game file assets. The fixed-format, 24-byte little-endian header:

| Offset | Type | Value |
| ---: | --- | --- |
| 0 | ASCII[8] | `M95LIGHT` |
| 8 | uint32 | Native image width |
| 12 | uint32 | Native image height |
| 16 | uint32 | Record size, exactly 5 bytes |
| 20 | uint32 | Reserved, 0 |

Each *row-major* 5-byte record at offset `24+5*(y*width+x)` stores:

| Offset within record | Type | Meaning |
| ---: | --- | --- |
| 0 | uint16 LE | Exactly sampled texture RGB565 **before** modulation/dither |
| 2 | uint8 | Applied interpolated/clamped red multiplier, 0..255 |
| 3 | uint8 | Applied interpolated/clamped green multiplier, 0..255 |
| 4 | uint8 | Applied interpolated/clamped blue multiplier, 0..255 |

Use the **seventh `_sourceaudit.bmp` 8/16/4-bit source flags** to
identify valid final ordinary textured MSI writes. A nonmodulated texel
records neutral multipliers `255,255,255`. HUD, untextured flat/shadow
and unowned pixels have an all-zero record, and subsequent writes
overwrite the previous pixel's record; keyed/discarded draws never
become last successful writers. The existing eight RGB565 BMPs remain
unchanged and use the same serial; all nine filenames participate in
non-overwriting `CREATE_NEW` collision handling.

```sh
python tools/matrox/analyze_lighting_capture.py game/matrox_capture_0010
python tools/matrox/analyze_lighting_capture.py game/matrox_capture_0010 --json
python tools/matrox/test_analyze_lighting_capture.py
```

The stdlib-only offline analyzer first verifies the previous eight
layers, then uses exact native source samples and per-pixel RGB
multipliers to *independently reproduce* the CPU renderer's existing
round-to-nearest RGB565 modulation:

`round(channel_5or6 * coeff_8 / 255)` as integer `(numerator+127)//255`.

When ordered dithering is active, it reproduces the precise 4x4 Bayer
threshold used by this **experimental software renderer**, with
`rem>threshold` deciding whether to round up. For non-alpha last
writes, the actual framebuffer and eighth undithered reference must
match these independent calculations **exactly** or the analyzer
rejects the trace. For alpha last writes, the earlier background
framebuffer is not stored, so it skips the exact final composite check
while retaining the lighting data. Reports include black rate by
mean RGB coefficient 0..31, 32..63, ..., 224..255; actual RGB coefficient
variation, per-index attribution and per-pixel consistency.

**Limits**: Only verifies mathematical consistency of this emulator
with the modifier values received from the original game. It does
**not** prove the Matrox Mystique hardware's actual integer precision,
color scaling, alpha, gamma or dither hardware rules. The 1996 MSI
SDK advertises Gouraud/textured triangles, not a particular channel
rounding algorithm. This branch does not change default settings,
shadows, raw palette, texture keying, modulation, dither, scene content,
or displayed game pixels. Additional optional diagnostic memory is
5 bytes/native pixel (~1.28 MB at 640x400) and is updated only on F12.
No screenshots, copyrighted game data or logs are committed.
