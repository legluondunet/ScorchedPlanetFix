# Matrox Mystique OEM support — proposed v2.1 integration

Status: **functional experimental Windows CI build**, not yet included in a stable release. The clean-room MSI95 v0.35 DLL is distributed alongside the test launcher; embedding it into the final single-file launcher is the next step. This document
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
   ISO9660/Joliet importer accepting Mode1/2352 raw CloneCD IMG and standard
   2048-byte ISO sectors. Strict 66-file size and SHA-256 verification, no
   arbitrary files accepted, staging and rollback before publication.
   `--matrox-install image.iso|image.img` installs and plays;
   `--matrox-install-only` installs originals without playing; an unambiguous
   Matrox image beside the launcher is auto-detected on fresh installs.
3. **Prototype package (from CI + tested clean-room v0.35):** Windows x64
   `ScorchedPlanetFix.exe` alongside the verified 100,352-byte x86
   `ScorchedPlanetFix_MSI95_v035.dll` (SHA-256
   `0d31c8f95e0387c959a59c57d57f29f4b599cd9b3469b0e2fce3383982a3946e`).
   The DLL is not a proprietary original Matrox driver.
4. **Pending:** fold the clean-room MSI95 source and reproducible x86 DLL build
   into CI; embed the DLL in the Rust launcher and remove the extra test file.
   Consider converting the experimental high-volume MSI95 logging/snapshots
   into production-level diagnostics. Add rollback/migration tests for runtime
   revisions.
5. **Pending end-to-end tests:** real Wine-staging, GE-Proton and Windows 11
   ISO/IMG installation, window focus, mouse guard, keyboard, Alt-Tab,
   cinematics, saves, and mission progression, plus 3dfx regression.
   **No release or merge into main until these pass.**

### Development commands

```powershell
# Original OEM already installed in scorchedplanet-matrox/
.\\ScorchedPlanetFix.exe --matrox-verify
.\\ScorchedPlanetFix.exe --matrox-prepare
.\\ScorchedPlanetFix.exe --matrox-play

# New OEM CD image in ISO2048 or IMG2352 format
.\\ScorchedPlanetFix.exe --matrox-install-only "Scorched Planet Matrox.iso"
.\\ScorchedPlanetFix.exe --matrox-verify
.\\ScorchedPlanetFix.exe --matrox-play
```

The optional clean-room DLL must currently be placed beside the launcher as
`ScorchedPlanetFix_MSI95_v035.dll` for preparation or gameplay. A `--verify`
command is strictly read-only and needs no DLL. The Matrox installer does not
touch `splanet/` or the 3dfx GameFrame wrapper.

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
selection), not accidental precedence. The experimental PR supports Matrox runtime installation and launch (pending owner Wine/Windows gameplay validation). Its current test package includes one separate clean-room DLL until automatic embedding is implemented.

No new proprietary assets were committed. The manifest contains only original
path names, sizes, and cryptographic fingerprints.
