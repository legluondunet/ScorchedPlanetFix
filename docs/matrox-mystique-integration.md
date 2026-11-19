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

| Edition | Executable | Backend | Native ratio | FPS setting |
| --- | --- | --- | --- | --- |
| Retail 3dfx | `spvoodoo.exe` | GameFrame `glide.dll` | 4:3 | `[scorch] FPSLimit` |
| Mystique OEM | `splanetw.exe` | clean-room `msi95.dll` | 8:5 | `[scorch] FPSLimit` |

Both backends should be packaged in **one downloadable Windows x64 launcher**,
with the appropriate x86 DLL unpacked into the correct game directory at
installation time. The launcher itself remains built in Rust. No proprietary
game or driver binary may be bundled.

## Implementation status (PR #36)

1. **Completed, Windows CI-validated:** committed exact OEM manifest; edition-neutral
   read-only `--verify`; separate `scorchedplanet-matrox/` and
   `scorchedplanet-matrox-runtime/` trees; exact hash-locked Win95 and
   FadeLow runtime copy patch; MSI95 graphics flags; no 300-second
   diagnostic process kill. Existing 3dfx GameFrame behavior is retained.
2. **Completed, Windows CI-validated, runtime test pending:** native Rust
   ISO9660/Joliet importer accepting Mode1/2352 raw CloneCD IMG, standard
   2048-byte ISO sectors, and original CUE/BIN or CUE/IMG descriptors through
   the existing strict CUE data-track normalizer. The CUE and its referenced
   IMG are deduplicated during automatic discovery. Strict 66-file size and SHA-256 verification, no
   arbitrary files accepted, staging and rollback before publication.
   The regular no-argument command installs and launches automatically;
   `--install-only` installs originals without playing. An unambiguous Matrox
   image beside the launcher is auto-detected on fresh installs.
3. **Completed, Windows CI-validated:** reviewed freestanding C source in
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
   directory, edition verification accepted all 66 original files / 315,947,520
   bytes, and the launcher started the clean-room MSI95 runtime.
   The driver recorded 3,305 frames / 68,496 ms, 1,114,647 triangles,
   291,970 Z-planes, 738,571 modulated textured triangles, 972 FPS-cap
   waited frames at frame 3300, and an orderly `msiExit` with
   `SHUTDOWN_COMPLETE elapsed_ms=5`. No first-chance exceptions reported.
7. **Still pending:** Windows 11 and Proton end-to-end runs; Alt-Tab,
   cinematics, saves, full missions, performance and 3dfx regression tests.
   **No release or merge until these pass.**

### Common user commands — identical for 3dfx and Matrox

```powershell
# Use the existing installation, or detect, install and launch the edition on disc
.\ScorchedPlanetFix.exe

# Detect and install a supported disc without launching
.\ScorchedPlanetFix.exe --install-only

# Verify the selected existing edition without changing files
.\ScorchedPlanetFix.exe --verify

# Other retained commands
.\ScorchedPlanetFix.exe --inspect "Scorched_Planet.cue"
.\ScorchedPlanetFix.exe --verbose
.\ScorchedPlanetFix.exe --launch-no-verify
.\ScorchedPlanetFix.exe --help
.\ScorchedPlanetFix.exe --version
```

The launcher searches beside itself, independently of the shell's working
directory. For the Matrox OEM CUE/IMG media, the CUE's data track is resolved
automatically and case-insensitively on Linux/Wine. It recognizes exact supported
disc profiles; modified or unrecognized media are refused. Only **one game edition** is managed per ScorchedPlanetFix folder. An existing
installation is always reused and launched; the CD image is ignored for
installation while an edition is already present. If both retail and Matrox
installation directories appear, the launcher refuses to proceed, leaving
both untouched. There is **no edition selection or edition-specific CLI flag**.
Multiple distinct candidate CD images are also refused rather than guessed. Neither original OEM files nor the original CD image are modified.

The clean-room MSI95 DLL is now **embedded in the executable**. No separate
driver file is needed. A `--verify` command remains strictly read-only. The
Matrox installer never touches `splanet/` or its 3dfx GameFrame wrapper.

## Unified CrimsonSkiesFix-style installation layout (v2.1 candidate)

```text
ScorchedPlanetFix/
├── ScorchedPlanetFix.exe
├── ScorchedPlanetFix.ini            # FPSLimit in [scorch], Dither in [matrox]
├── ScorchedPlanetFix.log            # selected runtime, reset on each game start
├── ScorchedPlanetFix_installer.log  # launcher session, noncumulative
├── game/                            # ONE installed edition, all game resources
│   ├── spvoodoo.exe / splanetw.exe  # exactly one playable EXE
│   ├── glide.dll / msi95.dll        # exactly one open renderer
│   ├── splanet.ini                  # untouched native game preferences
│   └── ...                          # original game data and player saves
├── state/
│   └── ScorchedPlanetFix.install-receipt.txt
└── docs/
    ├── README.md
    ├── LICENSE
    ├── ScorchedPlanetFix.ini.example
    └── build-info.txt
```

- The executable, installer log, runtime log and fix-owned INI are together at
  the launcher root, not in `game/`. The game retains its own native
  `game/splanet.ini` for language, settings and videos.
- Retail 3dfx: `game/spvoodoo.exe`, original authenticated
  `game/spvoodoo.exe.bak` and embedded GameFrame `game/glide.dll`.
- Matrox Mystique: `game/splanetw.exe`, original authenticated
  `game/splanetw.exe.bak`, embedded clean-room `game/msi95.dll`,
  and original proprietary `game/msi95.dll.bak` retained as DATA ONLY,
  never loaded by the game. All remaining 64 OEM immutable resources retain
  their exact original hashes and paths. This replaces the former separate
  source and runtime trees; game copies and saves are never duplicated.
- No `dlls/` or `third_party/` directory is created. Windows/Wine finds
  the required DLL alongside the active executable without search-path hacks.
- `state/` contains only fix-owned receipt/installation metadata.
  `docs/` contains the reference files and example configuration.
  Preexisting player-owned files are not overwritten.
- Old `splanet/`, `scorchedplanet-matrox/` and
  `scorchedplanet-matrox-runtime/` layouts are **not migrated
  automatically**: if found beside the launcher the new version refuses
  to run or install over them. Move to a separate fresh test directory.
- `--verify` remains read-only, and `--launch-no-verify` never repairs or
  copies runtime files. The ordinary launcher verifies the sole edition
  detected inside `game/` and can replace only trusted fix-owned DLLs.

## Shared INI configuration

```ini
[scorch]
FPSLimit=60

[matrox]
Dither=0
```

This is **ScorchedPlanetFix.ini** beside the EXE, not the original
`game/splanet.ini`. FPSLimit applies to GameFrame and MSI95; 0 disables
the limiter, 1–240 caps maximum FPS. Dither=1 enables the experimental
Matrox-only RGB565 ordered dithering; Dither=0 is the default. Unknown
settings are preserved; malformed or duplicate known settings are rejected.

See [unified layout implementation notes](unified-layout.md) for validation.

## 2026-11-19 — Matrox native defaults, no automatic BMP

- The player supplied a working original `splanet.ini` with
  `language= 1`, `HighQualityVideo= 1`, `gamma= 1.300000`,
  `settings= 0 2 1 1 5 95 127 0 0 5` (maximum tested graphics/shadows).
  Only fresh installs and exact unchanged installer-generated old INIs
  are updated. Custom player native INIs are preserved.
- The launcher INI now stores `[Graphics] FPSLimit=60` on both editions,
  and still supports Matrox-only `[matrox] Dither=0`.
- Automatic MSI95 BMP snapshots disabled; manual F12 remains opt-in.
- `[Cheats] UnlimitedFuel=0` is added as a reserved default; no verified
  unlimited fuel patch is yet available and `=1` is explicitly refused
  until a native game code patch has been validated.

### Native keyboard cheats (development, 2026-11-19)

`[Cheats]` in `ScorchedPlanetFix.ini` now documents
`ALIAH=0` (invulnerability) and `FATAL=0` (weapons and full ammunition),
both disabled by default. These are recognized as published keyboard cheat
codes for Scorched Planet, not verified executable patches. Sources disagree
whether invulnerability is spelled ALIAH or ALLAH. No automatic keyboard
injection is yet implemented; turning either INI option to 1 is refused with
an explanatory error, rather than silently failing. For now type the
appropriate code manually **during gameplay**.
