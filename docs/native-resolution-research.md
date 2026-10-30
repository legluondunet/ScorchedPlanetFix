# Scorched Planet 3dfx — native-resolution feasibility study

Status: **read-only offline analysis, not a resolution patch** (2026-10-30).

Goal: find the actual dimensions of the game render target and the interface
boundary with its RenderWare/3dfx driver, without disrupting the already
user-validated FadeLow timer fix. This workstream is independent of the
installer and media-check PRs.

## Exact game executable examined

- Original: `SPVOODOO.EXE` — PE32/i386, SHA-256
  `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a`.
- Compatible known-good FadeLow SHA-256:
  `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45`.
- PE image base `0x400000`; `.text` RVA `0x1000`, raw offset `0x400`.
- Both binaries pass the identical graphics-signature verification.
- The supplied binary was disassembled locally; **RWDLVOOD.DLL itself
  has not been supplied for this research stage**. Its entry points,
  accepted screen modes, and Glide call arguments have not been confirmed.

## Confirmed resolution initialization path

| VA | Instruction/behavior | Relevance |
| --- | --- | --- |
| `0x4349F0-0x434A73` | `GetSystemMetrics`, compute centered 640 × 480 rectangle, `AdjustWindowRect`, `CreateWindowExA` | Game window itself is set up for 640 × 480 |
| `0x434A90-0x434A9A` | Passes string `rwdlvood` (at `0x4B8FC4`) to `0x49A930` | Dynamically selects RenderWare graphics device |
| `0x434B5C` | `mov dword ptr [0x5746D4], 640` | Shared rendering width |
| `0x434B66` | `mov dword ptr [0x528F2C], 480` | Shared rendering height |
| `0x434C4A-0x434C59` | Passes (width, height, 0) to factory `0x48E850` | Creates an engine graphics object at the initialized dimensions |
| `0x48E943` | `call dword ptr [ebx+0x2C]`, with `ebx=[0x4BBD7C]` | Driver interface/function table; exact callback semantics not yet established |
| `0x434E50` | Returns 1 only for (width=640,height=480,bpp=16) | A predicate; not by itself a global video-mode restriction |
| `0x4305B5` | Sole **direct** call to `0x434E50`; false result skips a mode-dependent fade branch | The predicate is associated with a specific display-state transition |
| `0x435840-0x4358EB` | Chooses centered interface regions, 320 × 200 or 640 × 400, depending on shared width | Separate UI coordinate system requiring adaptation |

`0x48E850` is called elsewhere (e.g. `0x486CAD`) to construct
graphics objects using other size values; therefore changing the
initial globals does **not** mean all such objects change together.

### Confirmed references inside the EXE

For the original, full-section `objdump -d -M intel` reports:

- **71 instructions** directly reference width global `0x5746D4`.
- **328 instructions** directly reference height global `0x528F2C`.
- **25 direct calls** to viewport/layout helper `0x4358F0`.
- **Only one direct call** to the specific `0x434E50` mode predicate.
- **Two direct calls** to engine object factory `0x48E850`.

These are **instruction reference counts, not independent locations
that must all be patched**. They show substantial coupling between
the screen dimensions and the UI. Any automatic claim that a two-constant
patch creates a working native 1920 × 1080 game would be unjustified.

### What is not yet known

1. Does `RWDLVOOD.DLL` enforce its own internal mode table,
   640 × 480 Glide resolution, frame-buffer limit, or screen pitch?
2. Does RenderWare's virtual device callback at `0x48E943` accept
   dimensions above 640 × 480 from the game?
3. Does the game draw its 3D camera directly in device pixels or
   rescale into a fixed logical viewport?
4. Which code computes projection matrices/FOV and which scales 2D
   menus, mouse hit testing, images, and Smacker playback?
5. Is the display pipeline using the original Glide entry points or a
   modified vendor-specific interface? `RWDLVOOD.DLL` and `Glide.dll`
   would resolve this directly.

A standard Glide 2.x API includes a concrete display-resolution parameter
in `grSstWinOpen`, so tracing the 3dfx driver's call sites is the
appropriate boundary rather than blindly replacing the game window size.
The Glide API's available modes vary with version and implementation;
nGlide output scaling is **not** equivalent to a higher internal render
target.

## Proposed development path

1. **Completed:** verified the exact `RWDLVOOD.DLL` SHA-256,
   PE imports, export, and call to Glide `grSstOpen` (details below).
   **Still needed:** the active nGlide `Glide.dll`.
2. Reconstruct device initialization and video-mode enumeration.
3. Prototype **800 × 600 internal 4:3** in an isolated copy only after
   verifying driver acceptance and the surface/viewport contract.
4. If it survives game startup, mission entry, cinematics and menus,
   test **1024 × 768**. Compare geometry, image stretch, mouse offsets,
   text, HUD, frame pacing and Alt+Tab.
5. Handle 16:9 only after projection/FOV and screen-space elements
   are understood. A native widescreen mode must preserve aspect,
   not merely stretch the 4:3 output.
6. Decide whether a minimal version-locked EXE patch remains practical
   or an optional 32-bit compatibility DLL is cleaner for mode changes
   and UI corrections.
7. Any experimental copy must retain FadeLow's known-good behavior,
   refuse unknown hashes, preserve the original, and be rollback-safe.

**STOP CONDITION (updated 2026-10-30):** The exact working
`RWDLVOOD.DLL` has now been inspected (details below). Do not generate
a resolution patch until the actual active **nGlide `Glide.dll`**
has been inspected and mode/pitch/viewport invariants are understood.

## Reproducible inspection

`tools/inspect_native_resolution.py` is read-only and uses the
Python standard library; if `objdump` is installed, it also counts
direct instruction references. Its `--self-test` uses a synthetic
PE fixture in GitHub Actions; no commercial files are uploaded.

Run locally:

```bash
python3 tools/inspect_native_resolution.py /path/to/SPVOODOO.EXE
python3 tools/inspect_native_resolution.py --self-test
```

Related research branches: FadeLow PR #2, CD/INI PR #3. None is merged.

## Received RWDLVOOD.DLL: direct inspection (2026-10-30)

The project owner provided the **exact RenderWare 3dfx driver** from
the working game installation:

| Property | Value |
| --- | --- |
| SHA-256 | `1bd95e0dfeaaa47c53d5e2d4206027968790b928949321efd1eb2bb70f9201de` |
| File length | 101,376 bytes |
| Format | PE32/i386 DLL, 8 PE sections |
| Link timestamp | 1996-09-03 16:49:05 (PE header) |
| Image base | `0x10000000` |
| Only exported name | `_rwdev` (RVA `0x2530`) |
| Graphics dependency | `glide.dll` imported directly by decorated stdcall symbols |

**Important: this build imports the historical `_grSstOpen@24`**,
not the later `_grSstWinOpen` API. It also imports drawing, texture,
fog, buffer swap, hardware detection and LFB functions, including
`_grLfbGetWritePtr@4` and `_grLfbWriteMode@4`. A replacement
Glide wrapper must export the correct decorated names.

### Specific locked mode values

| DLL VA | Raw file offset | Disassembly / role |
| --- | --- | --- |
| `0x100013F3` | `0x7F3` | `push 7` immediately before `call 0x10008F3A` (`_grSstOpen@24` thunk) |
| `0x10001453` | `0x853` | `push 480; push 640` passed to internal RenderWare setup `0x100055F0` |
| `0x10001939` | `0xD39` | write width=640 to graphics mode descriptor |
| `0x10001946` | `0xD46` | write height=480 to descriptor |
| `0x10001953` | `0xD53` | write color depth=16 |
| `0x10001960` | `0xD60` | write `0x800` (2048) to another descriptor field; **not yet identified as a pitch** |

The `_grSstOpen@24` thunk at `0x10008F3A` jumps through the
Glide import address table at `0x10026380`. The Glide resolution
enumeration maps `7` to `GR_RESOLUTION_640x480` and `8` to
`GR_RESOLUTION_800x600`; this is documented in public Glide
source code / headers. The driver's mode number is therefore
not merely an arbitrary literal.

**Consequence:** modifying only `SPVOODOO.EXE` dimensions cannot
request 800x600 from Glide: this driver explicitly requests 640x480
and advertises the same dimensions in its device descriptor.
An internal 800x600 experiment would require coordinated updates
at the EXE/window, RenderWare DLL, Glide mode setting, device
surface properties and likely UI coordinate assumptions.

### Required next input

The user reports having the CD's original `Glide.dll` **and**
a newer `Glide.dll` supplied by nGlide, but is currently using
**nGlide**. The nGlide DLL from the exact working installation
is the correct next input to inspect: exports / supported
historical `_grSstOpen@24` ABI, implementation/version metadata,
and behavior at the interface. The CD-era DLL is optional
for historical comparison, not needed to start.

**Updated stop condition:** The exact RWDLVOOD DLL is now inspected.
Do not generate or ask the owner to test an experimental mode
patch until the **active nGlide `Glide.dll`** has been inspected
and the cross-module resolution assumptions are mapped.

