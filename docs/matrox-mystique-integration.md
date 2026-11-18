# Matrox Mystique OEM support — proposed v2.1 integration

Status: **draft implementation**, not yet included in a release. This document
tracks the migration of the experimentally validated standalone Matrox MSI95
adapter into the existing Rust-based `ScorchedPlanetFix.exe`.

## Evidence and scope

- User-owned 1996 Matrox Mystique OEM CD: ISO9660 ISO and CloneCD Mode1/2352
  IMG verified; 66 original files, 315,947,520 bytes, exact SHA-256 manifest
  committed in `data/cd-inventories/scorched-matrox-mystique-oem.sha256`.
- OEM `splanetw.exe`: 840,704 bytes, SHA-256
  `1f2684205f4aa349183acfef6eb660e92e6bb24732e5c56278f0df1111108894`.
- Original OEM `msi95.dll`: SHA-256
  `71b9921d8225403299fc73932a49ce5d3c1261a3bf7a852a3659d3f81cee3d9e`.
- Standalone diagnostic v0.35 ran under Wine with patched Win95 OS check,
  FadeLow, bounded 60 FPS, centered 8:5 borderless output, original 8-bit
  transparency, perspective correction, once-per-pixel shadow compositing and
  MSI95 per-vertex texture modulation. Ordered dithering remains optional
  and unvalidated visually.
- Validated **under Wine** on the tested OEM CD. Do not claim Windows native,
  other Matrox media, all missions, or performance parity with the 3dfx backend.

## Two independent renderers

Keep all current 3dfx/GameFrame code and checks intact. Never feed OEM
`splanetw.exe` through the 3dfx `spvoodoo.exe` patch profiles, never overwrite
its `glide.dll`, and never load the original proprietary Matrox `msi95.dll`
in the modern runtime.

| Edition | Executable | Backend | Native ratio | FPS ceiling |
| --- | --- | --- | --- | --- |
| Retail 3dfx | `spvoodoo.exe` | GameFrame `glide.dll` | 4:3 | 60 |
| Mystique OEM | `splanetw.exe` | clean-room `msi95.dll` | 8:5 | 60 |

Both backends should be packaged in **one downloadable Windows x64 launcher**,
with the appropriate x86 DLL unpacked into the correct game directory at
installation time. The launcher itself remains built in Rust. No proprietary
game or driver binary may be bundled.

## Stepwise implementation

1. **Manifest and installation recognition (this draft).**
   Add strict read-only SHA-256 verification of the 66 original OEM files.
   `--verify` recognizes `scorchedplanet-matrox/` only when the existing
   `splanet/` 3dfx installation is absent. A normal launch explicitly refuses
   to fall back to the 3dfx installer for that Matrox installation.
   All existing 3dfx paths remain unchanged.
2. **Unified installation.** Extend the Rust ISO/CUE installer to detect the
   Matrox OEM by original file fingerprints; handle raw CloneCD IMG and ISO
   without trusting filenames or silently replacing an existing installation.
   Keep a local immutable-original receipt and a separate patched runtime.
3. **Bundle the clean-room MSI95 renderer.** Port the standalone C shim build
   to reproducible Windows x86 CI and embed its bytes with SHA-256 and PE
   verification, like the GameFrame backend. Do not distribute the original OEM
   Matrox driver. Keep the existing patch in an isolated runtime copy.
4. **Remove diagnostic constraints.** Replace the time-limited standalone
   v0.35 Go launcher with Rust process management; the installed game runs
   until Quit. Keep 60 FPS fixed and the validated rendering defaults:
   perspective, indexed key, shadow-once and texture modulation. Optional
   dithering requires a separate visual decision.
5. **Validation.** Windows 11 + Wine/Proton install/verify/launch and long
   gameplay; Alt-Tab, Quit, mouse guard, save files, videos, malformed/unknown
   CDs, error paths, migration, and 3dfx regression tests. Only then publish
   as 2.1.0.

## Installation layout during transition

```text
ScorchedPlanetFix.exe
splanet/                       # Existing retail 3dfx, unchanged
scorchedplanet-matrox/          # Original OEM files (immutable)
scorchedplanet-matrox-runtime/  # Future patched OEM + bundled open MSI95
state/                         # Matrox install manifest / receipts
ScorchedPlanetFix_installer.log
ScorchedPlanetFix.log
```

The two directories may coexist in future, but automatic selection when both
exist must be explicit and deterministic (edition flag or last successful
selection), not accidental precedence. The current draft handles read-only
verification of a Matrox-only install; **it does not yet offer Matrox playback**.

No new proprietary assets were committed. The manifest contains only original
path names, sizes, and cryptographic fingerprints.
