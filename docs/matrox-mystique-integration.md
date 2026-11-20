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
  without preserving the proprietary OEM `msi95.dll` as a backup.
  The remaining 65 original ISO entries, including `splanetw.exe.bak`,
  retain their authenticated hashes and paths. This replaces the former separate
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

## [Debug] Log default and authenticated Matrox backups (2026-11-20)

The root INI now provides `[Debug] Log=0` by default, with descriptive
comments for **every** available option, including `[Graphics]`,
`[Matrox]`, `[Cheats]`, and `[Debug]`. With `Log=0`, the Matrox
MSI95 renderer never creates or writes a runtime log; `Log=1` enables
noncumulative `ScorchedPlanetFix.log`. The root installer diagnostic log
remains active independently.

`game/splanetw.exe.bak` is the exact original 840704-byte Matrox OEM EXE
and is required by the current immutable-manifest SHA-256 audit. The
patched and unpatched executables are verified separately; there is no
fallback to the backup for launching. The proprietary OEM MSI95.dll is
authenticated from the CD and no longer kept as a backup.

## Automatic logging on first actual game play (2026-11-20)

The Matrox edition now creates both the normal installer log and the
MSI95 runtime log on the first **verified** game session even though
`[Debug] Log=0` is the default in `ScorchedPlanetFix.ini`.
A first-launch receipt is created under `state/` only after the game
has exited successfully. Subsequently `Log=0` suppresses new runtime
logging but retains the first diagnostic file; `Log=1` opts back in.
`--install-only` and failed starts never consume the first-run receipt.
The shared retail GameFrame backend follows exactly the same policy.

## Repeatable user-requested F12 diagnostics (2026-11-20)

The MSI95 shim now emits a *new paired set* each time the user presses
F12, instead of overwriting `matrox_manual_frame.bmp` and
`matrox_manual_coverage.bmp`:

```
game/matrox_capture_0001_frame.bmp
game/matrox_capture_0001_coverage.bmp
game/matrox_capture_0002_frame.bmp
game/matrox_capture_0002_coverage.bmp
```

Both images come from the same rendered frame. Existing files survive
multiple presses and process restarts. A half-written pair is also
protected; the scan skips a serial if either filename exists and the
final `CreateFileA(..., CREATE_NEW, ...)` never replaces previous data.
F12 keyboard repeats are ignored until the key is released.
No automatic BMP snapshots are enabled.

## Safe automatic clean-room MSI95 upgrade from earlier PR #36 builds

The 2026-11-20 F12 multi-capture DLL has a new binary hash. The former
migration procedure only recognized a single legacy driver hash and
rejected intermediate CI builds as `Modified or unsupported Matrox runtime`.

Now `prepare` authenticates **65 retained immutable original CD files**
(including `splanetw.exe.bak`; the proprietary OEM MSI95 was checked
on initial media extraction and is not retained) plus the patched
`game/splanetw.exe` check. Only after these checks does it
safely update/repair the installer-owned `game/msi95.dll`. A temporary
staged and rollback DLL is verified and cleared after success. This
is a generic safe update for later source revisions, without
hardcoding every transient CI-produced DLL SHA-256. Original backups,
saves, and custom `splanet.ini` are kept. The read-only `--verify`
option remains strictly non-mutating.

## Strict embedded-DLL replacement policy (2026-11-20)

The launcher manages `game/msi95.dll` as its own runtime component,
independently of the identity of whatever previously occupied the path.
After validating the 65 original files retained in the installed tree,
including `splanetw.exe.bak`, the patched EXE, and
native configuration safety, it uses the active DLL's SHA-256 only
to determine whether it is *identical* to the embedded version.
If yes, it **does not rewrite the file**. Otherwise, it installs the
embedded MSI95 DLL, irrespective of whether the previous DLL hash
is known to the project. Missing files are restored.
The replace-and-verify procedure has temporary rollback protection and
never leaves permanent backups of installer-owned files.
Non-regular files and symlinks are rejected for filesystem safety.
`--verify` remains read-only.

## GameFrame / MSI95 backup policy alignment (2026-11-20)

The retail GameFrame renderer now follows the installer-owned DLL policy:
replace any differing regular `game/glide.dll` with embedded GameFrame,
leave identical content untouched, and restore missing DLLs. No permanent
backup of the superseded active renderer is created. Its previous contents
are kept only in a temporary rollback path during verified replacement;
rollback is deleted on success. Any existing legacy `glide.dll.bak` file
remains untouched. Matrox does not keep any proprietary DLL backup:
an exact legacy `msi95.dll.bak` is cleaned up after verification.

## 2026-11-20: Matrox scene-transition null-pointer crash — diagnostic only

The user's most recent Wine 11.19 run successfully updated the launcher-owned
MSI95 DLL `6fd8e712…` to `be3ef0d0…`, authenticated the 66 original
files, and began game execution. The MSI95 runtime log records
`NO_GEOMETRY_BEGIN` at frame 480 followed by multiple texture-heap
allocations (43 close together at frame 481), and a first-chance access
violation at game code `0x00494965` reading `0x00000024` around frame
498 (10.1 seconds). The identical exception repeats, and Wine exits with
NTSTATUS `0xC0000005` (`-1073741819`). This is a genuine memory fault,
not an ordinary legacy non-zero quit code. No F12 was pressed according to
the log. Previously successful runs did not show this same no-geometry
transition; therefore different driver builds may not be the only variable.

A new **read-only exception diagnostic** collects the 32 bytes centered on
the faulting EIP as `AV_CODE before16_and_after16=` to permit x86 opcode
analysis. The launcher now reports the actual crash status accurately.
No speculative memory patches or silent AV swallowing are implemented.
**Root cause not yet established and PR #36 must not be merged.**

## Identified crash instruction (2026-11-20, second Wine test)

A second logged Matrox session reaches frame 1227 (~41.99 seconds)
before repeating the **identical** fault from an earlier session at frame
498: `EIP=0x00494965`, read from `0x00000024` with `ESI=0`.
The logged faulting bytes decode to `push esi; mov esi,[esp+8];
mov eax,[esi+0x24]`. The function receives a **NULL first argument**;
the caller return address is `0x004186AA`. This is a precise direct
null-dereference in the game executable, not a DLL installation failure.

The wrapper did not report any invalid allocation/texture-heap exhaustion,
and the user did not press F12. The repeated failure at different frame
counts means that we must diagnose the call site, not just add a time-based
hack. The next non-invasive MSI95 diagnostics are `AV_CALLER` and
`AV_CALLER_CODE` (48 bytes centered at the return address), written only
for debug-enabled first-chance access violations. Do not patch the game
or ignore the AV before interpreting the caller's code path. PR #36
remains draft/unmerged.

## Fixing the verified game null dereference (2026-11-20)

A previous runtime trace showed `0x00494965` reading address
`0x00000024` with `ESI=0` after a scene transition. Offline
disassembly of the exact OEM original `SPLANETW.EXE`, SHA-256
`1f268420...`, reveals the call chain:

```asm
0x0042DB32  mov eax,[0x004C7B5C]    ; unrelated state flag
0x0042DB37  test eax,eax
0x0042DB39  je 0x0042DB50
0x0042DB3B  mov eax,[0x004B98C4]    ; resource pointer, can be NULL
0x0042DB40  push eax
0x0042DB41  call 0x004186A0
0x004186A0  mov eax,[esp+4]
0x004186A4  push eax
0x004186A5  call 0x00494960
0x00494960  push esi
0x00494961  mov esi,[esp+8]
0x00494965  mov eax,[esi+0x24]      ; erroneous dereference BEFORE NULL test
0x0049497C  test esi,esi            ; existing NULL test, too late
0x0049497E  je 0x004949E9          ; existing handler (return zero)
```

The first argument is confirmed NULL in both traces; when the
`0x004C7B5C` state is nonzero but the separate `0x004B98C4`
resource pointer is zero, this incorrectly calls the cleanup function.
The binary's existing NULL handler at `0x004949E9` makes a targeted
guard preferable to an unconditional skip or invented object.

A **16-byte** precise, old-hash-locked patch is applied before execution:
13 originally unused `CC` bytes at file offset `0x93D53` (VA
`0x00494953`) become `test esi,esi; je existing_null_handler;
mov eax,[esi+0x24]; jmp resume`; the original three-byte dereference
at file offset `0x93D65` becomes `jmp code_cave; nop`.
The trampoline is statically validated with x86 objdump on an exact
copy of the authenticated original EXE. Both the original OEM and
previous patched game EXE fingerprints are known:
`original=1f268420...`; `previous=333de76b...`;
`guarded=52b5f604ad7037e29e8cc83244c86662a2e6896b1e4d024ac4c68f23d080a759`.

Initial disc extraction installs the guarded EXE directly.
On an existing verified OEM installation the launcher first audits
all 66 immutable disc resources and both original `.bak` files,
then permits upgrade **only from the exact previous patched
EXE fingerprint**, using a temporary stage and rollback. There is
no added permanent backup, no user saves/config changes, and
`game/splanetw.exe.bak` stays byte-identical to the original CD.
Unknown patched EXEs and incomplete rollback transactions are refused.

This eliminates the proven NULL dereference while retaining
the game's original NULL-handler behavior, but **must be checked
in live Wine/Windows gameplay before considering PR #36 mergeable**.

## No persistent DLL backups + repeated briefing clicks (2026-11-20)

The proprietary original `msi95.dll` is one of 66 hashes authenticated
against the user-owned Matrox ISO at initial installation, but is then
deleted from the private staging tree and replaced with the clean-room
renderer. The installed OEM audit therefore checks the other **65**
original CD entries (including the original `splanetw.exe.bak`).
No `msi95.dll.bak` is created or required. During upgrade, an obsolete
legacy backup is removed only if SHA-256 proves that it is precisely the
original driver; a foreign file is preserved without overwriting.

Repeated clicks on the mission briefing reproduce a new crash in
`splanetw.exe` at `0x004858CE` (`mov ecx,[eax+0x44]`) with `EAX=2`.
The original function takes an object pointer from its indexed list at
`[esi+0x0C]` without checking alignment. The experimental new patch
uses a 13-byte verified `CC` code cave at `0x00485853` and replaces the
three-byte dereference at `0x004858CE` with a short jump. If an entry is
not DWORD-aligned (the observed `EAX=2`), it is skipped through the
function's ordinary `ecx!=1` path; aligned pointers retain the original
`mov ecx,[eax+0x44]`. Total changed bytes: 16. The resulting exact
patched EXE SHA-256 is
`8bcded21d84b5d3cb8e4567bb32e6b038d168b632bdf081e4faebfcfdd50b710`.
The loader supports transactional upgrades from both earlier recognized
patched EXE hashes. This is not a proven resolution of the underlying
source of invalid list entries and requires live Wine/Windows tests.

## Root-cause candidate: MSI95 texture-heap tracking cap (2026-11-20)

Latest user's exact reproducible sequence: repeatedly click the mission
briefing. The `ScorchedPlanetFix(20261120-143534).log` records the renderer
at frame ~606 rejecting **41** 4-page calls to `msiAllocTextureHeap`,
then a genuine AV at `splanetw.exe:0x00482DEC` at frame 637,
`cmp [eax+edx*4],ebx`, with `EAX=0, EDX=5` and fault at `0x14`.
The previous crash session also experienced multiple allocation rejects.
The clean-room `msiAllocTextureHeap` implementation imposes
`g_heap_count >= 256` (unrelated to the game or hardware limitations).
Logged successful allocs minus tracked frees reach exactly 256.

The fixed-size `g_heaps[256]` was therefore a **demonstrable DLL bug**
and likely caused incomplete scene data. Replaced with a dynamically
growing VirtualAlloc-backed table; allocating 600 heaps is smoke-tested
on actual x86 Windows DLL to verify the 256/512 growth boundaries. Each
real texture allocation remains individually tracked for release.
The renderer now logs tracking growth, peak live count and REAL_OOM
separately from invalid requests; no arbitrary 256 allocation cap.

This avoids another speculative executable patch. The prior two Matrox
game EXE guards remain under evaluation; the source of the previous
null-object/invalid-entry crashes is not conclusively proven. Need
repeated-briefing-click Wine test and read the next log to confirm no
`msiAllocTextureHeap REJECT`/real OOM and stable mission entry.

## Scene-list teardown fault despite successful texture-heap growth (2026-11-20)

The updated MSI95 heap tracker passed a 600-allocation Win32 smoke
test and survived the user's rapid briefing clicks. The new runtime
log records 548 texture heap allocations with no refusal or OOM,
and successful 256→512 tracker growth. After ~4152 frames
(85.8 seconds), the user quit through the main menu and the game
crashed at `0x0048302D`: `cmp [ebp+edx*4],edi` with `EBP=0`,
`EDX=3`, reading `0x0C`. The log shows many successful
`msiFreeTextureHeap` calls immediately before the AV, and no
`msiExit` yet. The previously seen `0x00482DEC` is another
object-list removal instruction whose base table pointer is NULL
despite a positive list count.

The unmodified original EXE shows `[manager+0x0C]` is the
item-array pointer; `[manager+0x1C]` is the element count;
`[manager+0x20]` is involved in resizing. There is no evidence
that the MSI95 texture allocations failed in this session.
More EXE patches could conceal the actual lifecycle issue.

New read-only `AV_SCENE_LIST` telemetry collects these specific
fields and the manager flags/head for both fault addresses,
plus `AV_HEAP_TRACKER` and the live count after successful MSI95
frees. Previous generic `AV_CALLER` output was misleading on
non-prologue instructions and is limited to its originally
validated null-argument function.

The exit crash remains OPEN. The prior briefing guard remains
experimental; it may mask rather than fix underlying structure
inconsistency. PR #36 must stay draft/unmerged until reliable
gameplay + menu shutdown tests pass.
