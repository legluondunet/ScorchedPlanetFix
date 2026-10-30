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
WINEPREFIX="/path/to/your/wine-prefix" \
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

### Second real capture: ptrace denied (2026-10-30, 10:05)

The user-supplied log records `CLK_TCK=100` and main Linux TID
`389977` accruing **515 CPU ticks in roughly five seconds**. The
Wine prefix was confirmed by the diagnostic tool. It also confirms
`ptrace: Operation not permitted` when regular GDB tried to attach
to the process. No instruction pointer was obtained.

Unlike the previous capture, the process was identified as
`/usr/lib/wine/x86_64-unix/wine-preloader`; i386 Wine modules came
from `/usr/lib/wine`, while certain GE-Proton font/theme resources
were also mapped. A mapped resource path alone does not establish
which Wine runner actually launched the game.

The first GDB capture failed, and the script did not attempt the second.
The earlier implementation did not log whether the process had exited;
this is not evidence that GDB killed the game.

The script now includes an **explicit opt-in** `--gdb-sudo` mode:
before entering fullscreen, it asks for sudo authentication, then later
runs the two GDB captures using `sudo -n gdb`. This requires a trusted
local debugger and local sudo privileges. It does **not** change
`/proc/sys/kernel/yama/ptrace_scope` or write any system settings.

```bash
cd /path/to/installed/scorched-planet
WINEPREFIX="/path/to/your/wine-prefix" \
  bash ./diagnose-3dfx-hang-linux.sh --wait --delay 30 --interval 5 \
  --game-dir . --gdb-sudo
```

Alternatively first inspect the read-only policy from a terminal:
`cat /proc/sys/kernel/yama/ptrace_scope`. Yama scope 1 restricts
unrelated process attachment by default; other policy and privilege
conditions can also produce EPERM. If sudo is not desired, preserve
existing system policy and postpone instruction sampling until a
debugger can launch the Wine process as its child while retaining the
normal Lutris runtime environment.

### Third real capture (2026-10-30, 10:14): dual GDB succeeds

The user-provided main `.log` confirms two privileged GDB captures
at 10:14:38 and 10:14:41. The Linux process has
`WINEPREFIX` confirmed, and the principal thread (TID 417981)
accumulated 515 CPU ticks in the five-second comparison window.

**The primary log format at that time did not include the actual
program counters or stack frames.** Both complete captures were saved in
the timestamped details directory as `gdb-sample-1.txt` and
`gdb-sample-2.txt`; those files are needed to assign the busy
thread's instruction address to SPVOODOO.EXE, RWDLVOOD.DLL, Glide.dll,
Wine, or the graphics driver.

The Bash diagnostic now includes the matching Linux TID's PC,
nearby disassembly and backtrace sections in the top-level `.log`
on future captures, while retaining the full detail files.
The existing captured details are preferable to re-running the game
solely to regenerate a more complete summary.

### Two complete GDB snapshots analyzed (2026-10-30)

The two 162,300-byte GDB captures are **byte-for-byte identical**.
SHA-256 of each: `b5989531615f0444a089e9b0ecca602a436a6bc9dc93c628f7620da4195d6d35`.

Main Linux TID `417981` is stopped in both samples at
`0x00007f596854c192` in native Wine `ntdll.so`
(mapped 0x7f5968513000-0x7f5968583000). The disassembled instruction is
`stmxcsr 0xd8(%rcx)` (saving the SIMD control/status register).
The next call-stack frame `0x00006ffffc3a1526` maps to Wine's
64-bit `wow64.dll`. GDB warns about incompatible architecture
`i386:x86-64` vs `i386:x64-32`, limiting visibility into 32-bit game code.

**Do not patch ntdll.so or assume the busy loop lives there.** GDB attach
may stop in Wine's thread-context save code. An identical suspended RIP
does not locate the actual hot instruction, even though the `/proc`
CPU accounting independently shows a busy main thread.

Next step: **Linux perf sampling** of instructions while the game is running.
Unlike GDB attachment, perf collects asynchronous CPU samples without
repeatedly stopping the game at a debugger attach location.

```bash
cd /path/to/installed/scorched-planet
WINEPREFIX="/path/to/your/wine-prefix" \
  bash ./diagnose-3dfx-hang-linux.sh --wait --delay 30 --interval 5 \
  --game-dir . --perf-sudo
```

The opt-in `--perf-sudo` obtains sudo authorization before fullscreen,
then samples the main Linux TID for five seconds with user-mode CPU-clock
events (`perf record -e cpu-clock:u -F 99 -t PID`). It saves
`perf-report.txt`, `perf-samples.txt`, and a local `perf.data` in the
timestamped details folder, and inserts a concise summary in the main log.
Collection may fail due to kernel perf policy, missing tool, or perf
capabilities. The script neither changes kernel policy nor patches any
Wine/game files. Review metadata/paths before sharing; send text first.

### Fourth real diagnostic (2026-10-30, 10:26): game exits before perf sampling

The run used `--perf-sudo` with the correct Wine prefix. It captured
`snapshot-1` at **10:26:39**, then reported **game terminated before
`snapshot-2`** at 10:26:44. The original script returned before its
perf stage, so **no perf command ran**, and therefore `perf-report.txt`,
`perf-samples.txt`, and `perf-record.txt` were not generated. This is
a control-flow issue in the diagnostic script, not evidence of a perf
installation/permission issue.

The first snapshot caught the principal process (Linux TID 427298)
in running state `R`, with its accumulated CPU accounting high.
`xrandr --current` showed desktop mode 1920x1080 at 120 Hz;
no physical X11 resolution switch was observed in that snapshot.
The process disappearance can reflect a crash, user exit, or an
external termination; a process status snapshot does not reveal why.

Fix: when `--perf-sudo` is requested, the tool now starts perf
**immediately after snapshot-1**, for the configured `--interval`
seconds, then captures `snapshot-2` if the target still exists.
If the target exits during perf, the script still tries to generate
reports from any recorded `perf.data` and keeps a partial log.
The main `.log` now captures errors from `perf-record.txt` as well.
Test coverage includes a mocked process terminating while perf runs.

### Fifth real diagnostic: Go freezes and music repeats (2026-10-30, 10:49)

User reports: from the game's main menu, clicking **Go** to start a new game
freezes the window; approximately the last four seconds of music continue
looping. This matters because the audio may be replaying a buffered fragment
while the main gameplay transition is stalled; **this does not prove** that
DirectSound is the root cause.

The latest `perf` run successfully sampled main Linux TID `446078` for five
seconds, with **178 samples and zero lost samples**. A summary of direct
sample counts (not inclusive call-stack CPU cost):

| Mapped image / category | Samples | Share |
| --- | ---: | ---: |
| native Wine `ntdll.so` | 76 | 42.7% |
| `/tmp/perf-446078.map` / perf JIT-coded image | 28 | 15.7% |
| Wine i386 `ntdll.dll` | 24 | 13.5% |
| Linux vDSO clock | 16 | 9.0% |
| Wine `wow64cpu.dll` | 16 | 9.0% |
| 32-bit `d3d11.dll` | 11 | 6.2% |
| remaining | 7 | 3.9% |

The particularly frequent runtime IPs are `0x7fdf3cb79192`
(39 samples), `0x7fdf3cb791df` (15 samples),
`0x7fdf3cf35c6c` / `__vdso_clock_gettime` (13 samples),
and several Windows-code addresses within `0x00432b00-0x00432bff`
(20 samples total, 11.2%). The Windows address range overlaps the
mapped `SPVOODOO.EXE` image range shown in the previous GDB snapshots
(base `0x00400000`), but perf labeled it JIT and an **exact module/code
attribution remains to be verified from the matching executable**.

The combination of recurring clock calls, WoW64 transitions and an active
main thread is consistent with polling or a tight timing loop; it is not
conclusive evidence of which function is faulty or whether the music loop
causes the freeze. No observed direct CPU samples were assigned to
`RWDLVOOD.DLL` or `Glide.dll` in this interval. A five-second main-thread
sample cannot exclude wrapper involvement.

**Next step: static code analysis**, without restarting the game:

```bash
cd /path/to/installed/scorched-planet
bash ./inspect-go-hotspots-linux.sh ./SPVOODOO.EXE
```

The read-only `tools/inspect-go-hotspots-linux.sh` verifies SHA-256 against
the already supplied retail `SPVOODOO.EXE` hash
`fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a`
and uses GNU objdump to extract import names, section VMAs and instructions
around `0x00432b62`, `0x0043610d`, `0x00427d38`.
Output is a standalone timestamped `ScorchedPlanetFix-Go-code-*.txt`
in the current/game directory. The tool neither modifies nor executes the
commercial game executable. If binary hash differs, it warns the perf
addresses may not map to the same build.

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
