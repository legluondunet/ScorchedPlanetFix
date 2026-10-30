# ScorchedPlanetFix — compatibility architecture

Status: **design approved by the project owner** (2026-10-30).
This document records the agreed strategy; it does **not** claim that the
CD or high-resolution fixes are implemented or tested.

## Guiding principles

- Preserve the original Scorched Planet (1996) game, RenderWare/3dfx
  engine, audio and game content whenever they remain functional.
- Prefer a **small, version-locked, reversible EXE patch** when a single
  condition, constant, or instruction sequence explains a problem.
- Introduce a **32-bit optional compatibility DLL** only when dynamic
  interception, configuration, window management, camera/FOV, or multiple
  related hooks make this simpler and safer than extensive binary edits.
- Avoid replacing `Glide.dll` by default: existing Glide wrappers (nGlide,
  dgVoodoo2) may already supply that name. Determine a robust load method
  after studying the executable's module imports and startup sequence.
- Keep original DirectSound and Smacker playback unless a reproducible
  failure requires an intervention.
- **No complete game decompilation/rewrite** in this project's near-term
  scope. A native modern engine port is a separate research project.
- Make all experiments opt-in, byte-verifiable, backed up, idempotent,
  reversible, and isolated from each other as much as possible.
- Keep commercial game binaries and proprietary assets out of GitHub.
- Use pull requests for changes; **do not merge without explicit owner approval**.

## Components

| Component | Role | Status |
| --- | --- | --- |
| `tools/scorchedplanetfix_patch.py` | Exact-SHA256 FadeLow copy/apply/restore | User-tested change; CI-tested tooling in draft PR #2 |
| `tools/diagnose-3dfx-hang-linux.sh` | Read-only Linux/Wine timing and process diagnostics | Research tooling |
| `tools/diagnose-3dfx-hang.ps1` | Read-only Windows diagnostics | Research tooling |
| Optional `ScorchedPlanetFix.dll` | Future configurable hooks and compatibility features | **Not implemented** |
| Rust CD installer (PR #1) | Modern install from user-owned original CD files | **Separate draft workstream; unchanged** |

## Next workstream: original-CD dependencies

Verified against the original user-provided retail `SPVOODOO.EXE`,
SHA-256 `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a`:

- It imports `GetDiskFreeSpaceA`, `CreateFileA` and
  `GetFileAttributesA`. The import directory does **not** contain
  `GetDriveTypeA`, `GetVolumeInformationA` or `DeviceIoControl`.
- It contains the path-format string `%ssplanet\\geometry.rwb` and
  the file name `GEOMETRY.RWB`.
- An unmodified call chain starting at `0x435760` iterates prospective
  drive letters, calls `GetDiskFreeSpaceA` indirectly through
  `0x57F368`, and performs two distinct file-opening attempts.
  One branch returns success at `0x435827`.
- This is **consistent with** locating game data and with a rudimentary
  read-only media check; full file-open semantics and all callers must
  be verified before treating this as an anti-copy check or modifying it.

Plan:
1. Reconstruct `0x435760` callers and the file-opening helper
   `0x4AD2A0`, recording which open modes the two calls use.
2. Record when the check runs (startup, mission loading, or both).
3. Distinguish media-presence detection from legitimate asset lookups.
4. If necessary, add a narrowly scoped optional change using the
   **same supported-executable hash, backup, and restore discipline**
   as FadeLow. Avoid bypassing resource loading.
5. Validate from a lawful complete installation with no mounted CD,
   including game start, several levels, music, movies and save/load.

**No CD change is implemented in this document.**

## Next workstream: high-resolution rendering

Offline inspection of the original `SPVOODOO.EXE` also confirms:

- `0x434B5C` initializes global dimensions with
  `640` (`0x280`) and `480` (`0x1E0`).
- `0x434E50` contains a function returning true only when its
  width/height/depth arguments are `640 × 480 × 16`. This specific
  mode gate is **not sufficient evidence** that all engine surfaces,
  rendering paths, and HUD geometry scale correctly at other sizes.
- The binary contains `hires\\` and `lores\\` data-path strings,
  but those do not establish a modern supported renderer resolution.

Plan:
1. Establish baseline output scaling with the currently working nGlide.
2. Trace render-surface initialization across `SPVOODOO.EXE`,
   `RWDLVOOD.DLL` and `Glide.dll` interfaces.
3. Try **4:3 internal high resolution** before widescreen; inspect
   framebuffer dimensions, depth, UI coordinate transforms, mouse,
   video playback, and image aspect.
4. Only after a stable internal mode exists, research optional native
   16:9 (projection/FOV, HUD anchoring and cinematic composition).
5. Decide experimentally whether a few exact EXE patches suffice or
   an optional DLL provides a better boundary; **do not hook `Glide.dll`
   blindly**.

**No internal-resolution patch or DLL is implemented yet.**

## Other future compatibility work

- Alt+Tab/fullscreen window focus changes (separate reproducible issue).
- Controller input normalization, only after controller behavior is measured.
- Audio/Smacker changes only to address confirmed defects.

## Release and merge gates

1. Confirm exact original file identity and preserve original contents.
2. Record reproducible baseline, modified behavior, and rollback behavior.
3. Check the game starts, enters a mission, maintains audio/video, survives
   multiple transitions, and quits cleanly.
4. Keep gameplay-test reports distinct from static evidence and simulations.
5. Confirm relevant GitHub Actions checks are green.
6. **Request explicit owner approval before merging any PR.**

Relevant: [FadeLow usage](fadelow-patch.md) and
[graphics diagnostics](graphics-diagnostics.md).
