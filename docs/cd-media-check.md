# Scorched Planet (1996): optical-media check research

**Status: experimental, NOT tested in the game without a CD.**

This workstream is separate from the already user-validated FadeLow
timing patch and from the draft Rust installer. It does not alter the
graphics or sound engines.

## Evidence from the exact original executable

Analyzed `SPVOODOO.EXE` PE32/i386 with SHA-256
`fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a`.

- At `0x435760` a function enumerates Windows drive letters and constructs
  `%c:\\`, then looks for `%ssplanet\\geometry.rwb`.
- It calls imported `GetDiskFreeSpaceA` through IAT `0x57F368`.
- The CRT `_open`-style helper at `0x4AD2A0` converts its low flag bits
  into `GENERIC_READ` / `GENERIC_WRITE` before calling `CreateFileA`
  through IAT `0x57F370`.
- At `0x4357DA..0x4357EA`, a first `open(path,0,0)` must succeed;
  at `0x4357F7..0x435806`, a second `open(path,1,0)` must fail.
- When the read succeeds and the write fails, the function returns
  `eax=1` via `0x435827`. When no candidate qualifies, it returns
  `eax=0` via `0x43581B`.
- Three direct call sites `0x42E70C`, `0x42E75A`, `0x42E7E4`
  store this return value to the global flag `0x52B284`.
- The flag is inspected by several menu/interaction code branches.
  **The code also initializes the same flag to 1 at `0x4334AE`.**
  Therefore a CD check does **not** necessarily run before every
  game startup, and may be limited to specific menu actions.
- `setup.ini` from the original installation defines
  `[Voodoo] WinAppFile=SPVOODOO.EXE`, `SizeData=63`,
  `CopyVideos=0`. A CD may contain actual resources still required
  by the game. Bypassing a media-presence flag cannot supply those files.

**Conclusion from static analysis:** this is consistent with a basic
read-only optical-media check, not positive identification of a
cryptographic SafeDisc/SecuROM scheme. It may be both a media check
and a resource availability flag.

## Pre-patch control experiment (important)

Before changing any EXE, **remove/unmount the original CD** and test
the already working `SPVOODOO-FadeLow.EXE` with the same Wine prefix
and nGlide configuration.

If FadeLow already launches and loads multiple missions without the
disc, **do not apply the extra CD experiment**: this check does not
need to be modified for that installation.

Only if the absence of the CD causes a specific failure should we
compare with an isolated test copy.

## Experimental, narrowly scoped binary modification

Our standalone, copy-only tool is
`tools/experimental_cd_media_gate.py`.

It accepts exactly two known inputs:

| Input | SHA-256 |
| --- | --- |
| Original retail 3dfx EXE | `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a` |
| FadeLow validated EXE | `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45` |

The experiment changes two bytes at virtual address `0x43581B` /
raw PE offset `0x34C1B`:

| Original | Experimental |
| --- | --- |
| `33 C0` — `xor eax,eax` (zero/false on failed search) | `B0 01` — `mov al,1` (nonzero/true on failed search) |

When a read-only candidate is found, the original success branch is
**unmodified**. On exhaustive search failure, the experimental path
still runs the original loop and cleanup, but makes the return value
nonzero.

**Caveat:** `mov al,1` changes the low byte but leaves high bytes
of EAX intact. This is intentional for a two-byte-only modification.
Every identified consumer of the stored flag `0x52B284` tests
whether it equals zero; the whole-word return is **not guaranteed to
equal exactly one**. Further uses not identified by static direct
cross-references could behave differently.

When generated from the validated FadeLow binary, the resulting
**FadeLow + NoCD test** SHA-256 is
`4dc4de1f9034e17b266da7b64a83ba897b4dca9e8cf9a8654f5988d02d1e0613`.
Relative to FadeLow, exactly two bytes change. Relative to the retail
original, six bytes differ (4 FadeLow + 2 CD experiment).

This trial does **not** add missing CD content, emulate a drive or
promise that video and music files will be found.

Generate a separate new file (the script refuses overwrites):

```bash
python3 tools/experimental_cd_media_gate.py \
  /path/to/SPVOODOO-FadeLow.EXE \
  --output /path/to/SPVOODOO-FadeLow-NoCD-TEST.EXE \
  --experimental
```

Use the normal Wine runner and nGlide. In Lutris, temporarily point
the executable at the alternate-name test file; do not rename, delete
or overwrite the working FadeLow file. Check game menus, new game,
save/loading, mission transitions, audio, movies and quit.

Rollback: select the previously validated FadeLow EXE again and delete
only the experimental extra file.

**Never merge or release as a production NoCD fix without a real
no-disc baseline and a positive test from the owner.**

## Follow-up

If the media-flag experiment does not allow play without a disc,
record the first missing file and its path. The likely next step is
fixing or configuring resource paths, not bypassing further checks.
The user must provide the in-game observation; static analysis does
not establish the missing assets or the environment's drive mapping.

## User no-CD baseline observation (2026-10-30)

**User report, before running the NoCD candidate:**

1. The working game installation is located under
   `/mnt/LUTRIS/Games/scorched-planet/drive_c/splanet`.
2. The user renamed the directory holding their original CD files from
   `/mnt/LUTRIS/Games/scorched-planet/drive_c/splanetcd` to
   `/mnt/LUTRIS/Games/scorched-planet/drive_c/splanetcd .bak`
   (the exact backup suffix may have been described informally).
3. With that directory absent at its expected name, **the game would not start**.
4. When the directory was renamed back to `splanetcd`,
   **the game launched normally**.

**Interpretation:** startup depends on the *presence or path* of
`splanetcd`, or on files / an emulated Wine drive that points into that
directory. The observation **does not yet distinguish** optical-media
presence checking from actual resource loading, nor does it prove a
cryptographic CD protection. The previous candidate only changes a
two-byte return-value instruction in a particular media-availability
probe. It cannot restore missing assets or broken drive mappings.

**Next controlled test:** keep `splanetcd` renamed to the backup name,
keep the same prefix, runner, graphics wrapper and game working directory,
and launch the privately provided
`SPVOODOO-FadeLow-NoCD-TEST.EXE` as a *separately named* executable.
If it launches, repeat mission, video, music and save/load checks.
If it fails at the same point, restore `splanetcd` and the working
FadeLow executable; inspect the first missing file / drive mapping
rather than developing further blind bypasses. Note whether "does not
start" means a missing-CD prompt, silent exit, crash, blank screen,
or menu displayed but Go failed.

**No result of this NoCD candidate trial has been received yet.**

