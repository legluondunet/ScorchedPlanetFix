# Scorched Planet — 3dfx / RenderWare startup hang

## Linux / Wine / nGlide (preferred for current testing)

The user has the commercial 3dfx game installed in a **Wine prefix under Linux**
with **nGlide**. The first diagnostics should therefore be collected on Linux;
the earlier PowerShell tool is retained for any future Windows tests.

The read-only Bash script reads the Linux process from `/proc`. It does not
need to know `WINEPREFIX`, does not reconfigure nGlide, does not modify
`SPVOODOO.EXE`, and never calls `wineserver -k`.

### Capture without Alt+Tab (recommended)

We observed another symptom on Wine/nGlide: after Alt+Tab, the game may
drop from fullscreen to a 640x480 window and stop accepting input.
This can be a separate focus/display-mode issue; do not confuse it
with the mission-start freeze until it has been diagnosed.

To avoid triggering it, **arm the monitor before starting the game**.
Open a terminal **in the installed game's directory** and run:

```bash
WINEPREFIX="/mnt/LUTRIS/Games/scorched-planet/" \
  bash ./diagnose-3dfx-hang-linux.sh --wait --delay 30 --interval 5 --game-dir . --gdb
```

With `WINEPREFIX` set, the script checks the process's exported
`WINEPREFIX` if readable, prefers a matching process during discovery,
and refuses a process whose reported prefix is definitively different.
It neither starts Wine nor changes the game's prefix. If the process
does not expose its prefix in `/proc/PID/environ`, the report explicitly
marks that association unverified rather than claiming a match.
Only the script process inherits this variable; start the game using
its existing correctly configured Lutris runner, separately.

The script waits for up to 120 seconds for the Linux process named
`spvoodoo.exe`. Now launch the game normally with the same Wine prefix
and nGlide settings (for example from Lutris). Start a mission without
using Alt+Tab. The monitor waits 30 seconds after it finds the process,
then takes snapshots five seconds apart. Change `--delay` as needed
for the time it takes to reach the menu.

If the game is **already running**, omit `--wait`, and use `--delay`
to schedule the capture before making it fullscreen:

```bash
bash ./diagnose-3dfx-hang-linux.sh --delay 20
```

Automatic process detection uses the Linux `comm` name. If your Wine
version uses another process name, find the Linux PID with:

```bash
ps -eo pid,comm,args | grep -i '[s]pvoodoo'
```

Then pass `--pid 12345` with the actual PID. This is the **Linux**
PID, not the PID reported by WineDbg.

The **main log is written directly in the launch directory**:

```text
./ScorchedPlanetFix-3dfx-YYYYMMDD-HHMMSS-pidNNN-shellNNN.log
```

Additional CSV-like text and TSV snapshots are saved in a distinct
subdirectory with the same basename, with no overwrite of previous
runs. This directory is created by the script; only the original game
files are left untouched. To select a different details directory,
use `--output`. The main log remains in the launch directory.

Optionally set `--game-dir .` to include SHA-256 hashes for local
`SPVOODOO.EXE`, `RWDLVOOD.DLL`, and Glide DLL files without copying
their contents. The script never starts or changes Wine itself.

The tool writes `summary.txt`, `process.txt`, thread/CPU snapshots,
`thread-cpu-delta.tsv` (per-thread CPU ticks accrued during the interval)
and loaded graphics-module paths. A high CPU delta can identify a
**busy thread**, while wait channels may indicate a waiting thread.
Neither observation alone identifies the root cause; the next step is
backtraces and instruction inspection.

Optional `--gdb` attaches native GDB **only after** the two noninvasive
snapshots, captures all-thread backtraces, then detaches. This can temporarily
stop or disturb a Wine process, may fail due to Linux ptrace restrictions,
and should not be used for the baseline run. If `gdb` is unavailable or
attachment is denied, the read-only snapshots remain useful.

Review captured local paths before sharing diagnostic text. Do not upload
full memory dumps or copyrighted game files. CI checks Bash syntax and
performs a smoke test against a harmless Linux `sleep` process; this does
not validate actual game compatibility.

---


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

### Evidence from first real Wine/nGlide test (2026-10-30)

The user confirmed a 5-second snapshot interval with `CLK_TCK=100`.
The Linux thread whose TID equals the process PID accrued **513 CPU ticks**
(approximately 5.13 seconds of CPU time), while nearly all other threads
accrued zero ticks. This is consistent with **one logical CPU being fully
occupied**. If the game was frozen throughout the measurement, an active
loop in the application/RenderWare/Glide stack becomes a high-priority
hypothesis, but the sample does **not** locate the loop or exclude a
normal high-CPU render loop in a responsive game.

The loaded process modules show:
- `SPVOODOO.EXE`, `RWDLVOOD.DLL`, and a local `Glide.dll`
- GE-Proton 11-11 Wine libraries loaded from a Steam compatibility-tools path
- 32-bit Vulkan/Radeon graphics stack, local `d3d11.dll`, MangoHud

The text log's `Wine=wine-11.17 (Staging)` reflects the interpreter in the
**script shell's PATH**, not necessarily the actual runner used for the game.
The diagnostic tool now records `/proc/<pid>/exe` as well.

**Next experiment:** capture two native GDB samples while the same hang is
present, without switching away from fullscreen:

```bash
cd /path/to/installed/scorched-planet
bash ./diagnose-3dfx-hang-linux.sh --wait --delay 30 --interval 5 --game-dir . --gdb
```

The script collects all-thread backtraces, instruction pointers, nearby
disassembly and process mappings in `gdb-sample-1.txt` and
`gdb-sample-2.txt` under the timestamped details folder. The top-level
`.log` includes an abbreviated overview. The GDB samples are opt-in;
ptrace policy or the runtime may prevent attachment. Do not disable
system-wide debugging restrictions merely for this test: send the
diagnostic error instead.

We will locate the busy thread in `info threads` (LWP = the Linux TID),
then map its PC to the actual executable or module before changing any
original binary. If GDB could not attach, inspect CPU and wait-channel
snapshots first and consider running the process under a debugger from launch.

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
