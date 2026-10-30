# FadeLow — Scorched Planet 3dfx timing fix

**Status:** the user reported successful gameplay after testing the **FadeLow**
variant under Linux/Wine/nGlide on 2026-10-30. This is a user-validated fix
for the reported freeze on clicking **Go**, **not a guarantee for other game
editions, wrappers, graphics adapters, or missions**.

This component is independent of the draft Rust installer (PR #1).
**No copyrighted game binaries are distributed**. You must already have your
own installed retail `SPVOODOO.EXE`.

## What it changes

The original game uses a high-resolution Windows performance counter for
palette fades and drops elapsed-time samples below a small threshold. Fast
polling can repeatedly discard elapsed time, preventing palette transitions
from completing. The FadeLow variant lowers the float32 threshold from
`0.0001` to approximately `0.00001` engine units.

| Property | Exact verified value |
| --- | --- |
| Supported retail file | `SPVOODOO.EXE` (PE32) |
| Original SHA-256 | `fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a` |
| FadeLow SHA-256 | `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45` |
| Target VA | `0x004495CD` |
| PE file offset | `0x489CD` |
| Original 4 bytes | `17 B7 D1 38` |
| FadeLow 4 bytes | `AC C5 27 37` |

No other executable bytes are changed. The tool refuses unsupported SHA-256
values, original instruction mismatches, missing backups for restore, symlinks
and unexpected existing output files.

## Linux (Lutris/Wine): recommended non-destructive copy

Close the game and go to the directory holding your original `SPVOODOO.EXE`.
Use Python 3.8+ and the patcher from this repository:

```bash
cd /mnt/LUTRIS/Games/scorched-planet/drive_c/splanet
python3 /path/to/ScorchedPlanetFix/tools/scorchedplanetfix_patch.py status
python3 /path/to/ScorchedPlanetFix/tools/scorchedplanetfix_patch.py copy \
    --output SPVOODOO-FadeLow.EXE
```

This creates a new executable without changing the original. **The output
name must not already exist.** If you already have the previously tested
`SPVOODOO-FadeLow.EXE`, keep it; you do not need to regenerate it.
Select the alternate executable in Lutris **Configure → Game options →
Executable** and keep the same Wine runner, prefix and nGlide configuration.

To undo this copy-only option, point Lutris back at `SPVOODOO.EXE`, then
optionally delete only the extra `SPVOODOO-FadeLow.EXE` file.

## Optional: apply to the original filename with a verified backup

For installations that require the exact filename `SPVOODOO.EXE`, you can
explicitly opt into replacement of that file. **Close the game first.**
The original is backed up **before** applying, and the tool validates the
backup SHA-256.

```bash
cd /mnt/LUTRIS/Games/scorched-planet/drive_c/splanet
python3 /path/to/ScorchedPlanetFix/tools/scorchedplanetfix_patch.py apply --yes
python3 /path/to/ScorchedPlanetFix/tools/scorchedplanetfix_patch.py status
```

Backup created beside the executable:
`SPVOODOO.EXE.ScorchedPlanetFix-original.bak`.

The file swap is atomic on the same filesystem. Applying twice is safe when
the original backup is still present; unknown game versions or corrupted
backup files are refused. If a valid backup already exists, it is retained,
not overwritten. **Do not delete the backup if you want automatic rollback.**

### Restore exactly the original bytes

With the game closed:

```bash
cd /mnt/LUTRIS/Games/scorched-planet/drive_c/splanet
python3 /path/to/ScorchedPlanetFix/tools/scorchedplanetfix_patch.py restore --yes
python3 /path/to/ScorchedPlanetFix/tools/scorchedplanetfix_patch.py status
```

Restoration only proceeds when both the installed FadeLow SHA-256 and the
backup's original SHA-256 match. The backup is preserved after restoration,
for safety.

## Windows

With Python 3.8+ installed, run the same commands in a terminal opened in
the game directory, replacing `python3` with `py -3`. **Close the game**
before `apply` or `restore`; Windows may refuse changes to a running EXE.
The patcher does not need administrator rights when you have write access to
the game directory. The copy-only method is preferred for first trials.

## Validation and known limitations

- The first real-game FadeLow test was reported successful by the owner.
- Local tests confirmed a byte-for-byte match with the previously tested
  FadeLow file and tested apply/restore against a separate copy of the actual
  original game EXE.
- CI runs 17 synthetic-PE tests: hashes, exact PE location, four-byte-only
  change, copy, backup, idempotence, restore and refusal of unsafe inputs.
- No copyrighted EXEs are checked into GitHub; CI uses synthetic files.
- The result is only verified for the exact supported retail SHA-256 and the
  user's Wine/nGlide setup. No other versions are patched automatically.
- Experimental `zero` mode exists in the research tool but is not integrated
  here; it was not needed for the reported fix.
- **No changes to the Rust installer in PR #1.** This patch remains in draft
  PR #2 until explicitly reviewed and approved for merge.

See [the static timing analysis](go-fade-timer-static-analysis.md) for the
underlying code analysis and remaining uncertainties.
