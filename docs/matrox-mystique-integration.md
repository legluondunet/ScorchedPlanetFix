# Matrox Mystique OEM support — proposed v2.1 integration

Status: **experimental dual-backend Windows CI build**, not yet included in a stable release. The clean-room MSI95 v0.35 driver is now built from source and embedded alongside GameFrame in one Rust launcher. This document
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

## Implementation status (PR #36)

1. **Completed, Windows CI-validated:** committed exact OEM manifest; strict Rust
   `--matrox-verify`; separate `scorchedplanet-matrox/` and
   `scorchedplanet-matrox-runtime/` trees; exact hash-locked Win95 and
   FadeLow runtime copy patch; 60 FPS/MSI95 graphics flags; no 300-second
   diagnostic process kill. Existing 3dfx GameFrame behavior is retained.
2. **Completed, Windows CI-validated, runtime test pending:** native Rust
   ISO9660/Joliet importer accepting Mode1/2352 raw CloneCD IMG, standard
   2048-byte ISO sectors, and original CUE/BIN or CUE/IMG descriptors through
   the existing strict CUE data-track normalizer. The CUE and its referenced
   IMG are deduplicated during automatic discovery. Strict 66-file size and SHA-256 verification, no
   arbitrary files accepted, staging and rollback before publication.
   `--matrox-install image.iso|image.img|image.cue [track.bin|track.img]` installs and plays;
   `--matrox-install-only` installs originals without playing; an unambiguous
   Matrox image beside the launcher is auto-detected on fresh installs.
3. **Completed, Windows CI-validated:** reviewed 116,216-byte C source in
   `tools/matrox/msi95_probe.c` (hash locked), x86 freestanding DLL compilation,
   explicit nine-export ABI audit, and embedded GameFrame + MSI95 in **one**
   Windows x64 executable. There are no proprietary game or Matrox drivers.
4. **Pending production polish:** reduce experimental MSI95 logging/BMP capture;
   stabilize safe upgrades for previously prepared runtime copies and audit
   performance across longer sessions. Add additional malformed CD/rollback
   tests and Windows 11/Wine 3dfx regression tests.
5. **First Wine-staging integration test completed (2026-11-18):** existing OEM
   source and prepared runtime passed 66-file SHA-256 validation, integrated
   Rust launcher started the game, and embedded MSI95 performed a complete
   resource cleanup after 6,514 rendered frames (133,968 ms, 2,409,833
   triangles, 654,991 Z-planes). The driver logged `msiExit` and
   `SHUTDOWN_COMPLETE`, with no first-chance access violations or errors.
   Legacy game exit code 1 was observed **with normal MSI95 shutdown** in
   this session; do not interpret code 1 alone as a crash or as proof of a
   successful session without the driver log. Startup mouse protection
   suppressed nine early events, becoming ready at frame 155; FPS cap was
   active at 60 maximum, with 1,171 paced frames at frame 6300.
6. **Fresh CUE/IMG install validated under Wine-staging 11.19 (2026-11-19):**
   The user-installed ISO9660 Mode1/2352 data track from
   `Scorched_Planet.cue` and its companion `Scorched_Planet.img`, without
   the standalone Go installer. The Rust launcher published the original OEM
   directory, `--matrox-verify` accepted all 66 original files / 315,947,520
   bytes, and `--matrox-play` started the clean-room MSI95 runtime.
   The driver recorded 3,305 frames / 68,496 ms, 1,114,647 triangles,
   291,970 Z-planes, 738,571 modulated textured triangles, 972 FPS-cap
   waited frames at frame 3300, and an orderly `msiExit` with
   `SHUTDOWN_COMPLETE elapsed_ms=5`. No first-chance exceptions reported.
7. **Still pending:** Windows 11 and Proton end-to-end runs; Alt-Tab,
   cinematics, saves, full missions, performance and 3dfx regression tests.
   **No release or merge until these pass.**

### Development commands

```powershell
# Original OEM already installed in scorchedplanet-matrox/
.\\ScorchedPlanetFix.exe --matrox-verify
.\\ScorchedPlanetFix.exe --matrox-prepare
.\\ScorchedPlanetFix.exe --matrox-play

# New OEM CD image in ISO2048, raw IMG2352, or CUE/IMG format
.\\ScorchedPlanetFix.exe --matrox-install-only "Scorched_Planet.cue"
# The CUE data track is resolved relative to the CUE file (case-insensitive).
# Optional explicit override: --matrox-install-only disc.cue disc.img
.\\ScorchedPlanetFix.exe --matrox-verify
.\\ScorchedPlanetFix.exe --matrox-play
```

The clean-room MSI95 DLL is now **embedded in the executable**. No separate
driver file is needed. A `--verify` command remains strictly read-only. The
Matrox installer never touches `splanet/` or its 3dfx GameFrame wrapper.

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
selection), not accidental precedence. The experimental PR supports Matrox runtime installation and launch (pending owner Wine/Windows gameplay validation) using one executable with both open-source renderers embedded.

No new proprietary assets were committed. The manifest contains only original
path names, sizes, and cryptographic fingerprints.
