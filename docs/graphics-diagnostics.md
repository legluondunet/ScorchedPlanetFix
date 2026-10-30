# Scorched Planet — 3dfx / RenderWare startup hang

Status: **diagnostic proof of concept**, not a fix. The installer work is separate.

## Confirmed symptoms and references

In the commercial Windows 3dfx edition, the game reaches its menu but can
freeze for tens of seconds (or indefinitely) when a mission is started or
a save is loaded. The issue has been observed with both nGlide and dgVoodoo2.

In 2019, nGlide author Zeus reported that this freeze also occurred on
Windows with fast CPUs and would require an EXE-side fix:
https://www.zeus-software.com/forum/viewtopic.php?p=8634

A modified RenderWare 3dfx driver, often distributed as
rwdlvood_lfbbypass.zip, exists **for the 3dfx demo**:
https://www.dosgamesarchive.com/download/scorched-planet
Applicability to the retail game has **not** been established.

None of this establishes that the freeze is caused by CPU speed, a busy
loop, a critical-section deadlock, or an invalid Glide call. We need
runtime evidence before making a binary patch.

## Capture two process snapshots (Windows 11)

Prerequisites: installed commercial 3dfx edition and Windows PowerShell 5.1.

Open PowerShell in the **repository checkout** and run (adapt game location):

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\diagnose-3dfx-hang.ps1 -GameDir 'C:\Games\ScorchedPlanet'

The script starts SPVOODOO.EXE. Start a mission. During a freeze, return
to the console and press Enter. Five seconds apart, the script captures:

- game-files.csv: SHA-256 + byte sizes of local game/render/wrapper binaries
- summary.txt: PID, process CPU totals, memory use, thread counts
- snapshot-1-threads.csv and snapshot-2-threads.csv: thread IDs, states,
  wait reasons, accumulated CPU time
- snapshot-1-modules.csv and snapshot-2-modules.csv: loaded module names,
  paths and base addresses

By default output goes to:

    %LOCALAPPDATA%\ScorchedPlanetFix\Diagnostics\<timestamp>\

The process is never killed by the script, and original game files remain
untouched. If the game terminates during the capture, the log records it.

### Isolate an affinity / timing problem

Repeat the same experiment (same wrapper, same graphics settings), adding
the SingleCore switch:

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\diagnose-3dfx-hang.ps1 -GameDir 'C:\Games\ScorchedPlanet' -SingleCore

This limits the game to logical CPU 0 for this run. It is **only a test**:
success would be evidence of a timing or concurrency sensitivity, not proof
of the root cause. Compare launch latency and whether the hang is reproducible.
Do not combine a wrapper change and an affinity change in one experiment.

### Optional: two full memory dumps

Install Microsoft's Sysinternals ProcDump separately (or supply its path).
Full dumps are **opt-in**:

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\diagnose-3dfx-hang.ps1 -GameDir 'C:\Games\ScorchedPlanet' -CaptureDumps -ProcDumpPath 'C:\Tools\procdump64.exe'

This invokes ProcDump twice on the hanging process. Inspect dumps locally
with WinDbg, starting with the all-thread stacks. If the same active
thread's CPU rises while its instruction pointer stays within a small
code region, investigate a busy loop; if threads remain in waits, trace
wait dependencies and graphics/audio calls. These are hypotheses, not
conclusions.

**Privacy:** paths in CSV/text may disclose local user account names.
Memory dumps may contain private data. Do not commit or publish dumps on
GitHub. Review and redact reports before sharing.

## Investigation order

1. Preserve pristine copies and record SHA-256 for SPVOODOO.EXE,
   RWDLVOOD.DLL and any local Glide/DDraw wrappers.
2. Record two snapshots of a baseline hang using one wrapper.
3. Repeat with single-core affinity, changing nothing else.
4. If unresolved, collect local dumps and inspect all thread stacks and
   instruction pointers (WinDbg / GDB under Wine).
5. Only once the blocking instruction/call is identified: propose a
   narrowly scoped patch or hook. Validate both original behavior and
   mission loading before modifying resolution or FOV.

Do **not** assume this is an API-wrapper bug solely because the freeze
happens around the transition from menu to 3D. Do not bundle unverified
3dfx DLL replacements or install vintage drivers system-wide.
