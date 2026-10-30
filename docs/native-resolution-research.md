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

1. **First obtain original `RWDLVOOD.DLL`** from the same working
   installation and check its SHA-256, PE import table, exports and
   dynamic calls to Glide; also inspect `Glide.dll` if needed.
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

**STOP CONDITION:** no experimental resolution binary is produced
until the **exact** working `RWDLVOOD.DLL` can be inspected.
No game test, user-visible resolution change, or driver compatibility
has been claimed.

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
