# Experimental original Matrox OEM exit-code trace

The original 1996 Matrox Mystique OEM **SPLANETW.EXE** can return process
status **1** after a normal menu exit on both Windows and Wine. The launcher
already accepts it. We want the **exact last Windows message** behind that
status without changing the game executable, MSI95 renderer or retail 3dfx.

## Static evidence from the owner's authentic 1996 CD

Verified independently from the original 840,704-byte OEM SPLANETW.EXE,
SHA-256: 1f2684205f4aa349183acfef6eb660e92e6bb24732e5c56278f0df1111108894
(PE32, ImageBase 0x00400000; never redistributed).

- 0x00416A73 calls PeekMessageA with MSG* at [esp+0x1C].
- 0x00416A79 checks PeekMessageA's EAX return value (message available).
- 0x00416A7D reads MSG.message at [esp+0x20].
- 0x00416A84 branches on WM_QUIT (0x12).
- 0x00416CB6 reads the game's own quit flag at 0x00691B28,
  allowing exit without WM_QUIT.
- 0x00416CEC reads [esp+0x24], exactly MSG.wParam (MSG at +0x1C, wParam at +8).
- 0x00416CFA returns this EAX from WinMain; the original CRT
  calls WinMain at 0x004A712B, then passes EAX at 0x004A7130
  into its process exit routine.

This demonstrates the return **mechanism**, not the actual message that had
wParam=1 during a user's observed session. Possibilities include key flags
from mouse events and activation messages, but none is yet confirmed.

## Live one-shot verification, no patch to OEM code or data

The optional, standalone **MatroxExitTrace.exe** is compiled for Windows
**x86**, from trace_oem_exit.rs. It is not bundled with the normal launcher.

It attaches with Windows debugger APIs, verifies three exact OEM instruction
signatures in memory and uses **CPU hardware execution breakpoints DR0/DR1**.
It does not write or replace game instructions, IAT entries, files,
or process exit status. It clears the debug registers and detaches after
the original WinMain return is captured.

1. Start the installed Matrox edition with the normal ScorchedPlanetFix.exe,
   wait for the game menu.
2. In a second Windows or Wine terminal running in the **same Wine prefix**
   as the game, simply execute:

       MatroxExitTrace.exe

   The tracer enumerates **Windows** processes, finds exactly one original
   SPLANETW.EXE, authenticates the known WinMain bytes and prints
   AUTO_DETECTED Matrox OEM Windows PID=... . For a more detailed history,
   optionally run MatroxExitTrace.exe --messages (slower).
   Supplying a PID is still supported, but Unix/Linux host PIDs under Wine
   are **not Windows process IDs**. Passing a Unix PID such as 35798 can
   make OpenProcess fail with ERROR_INVALID_PARAMETER (87). The tracer
   now uses its unique verified OEM Windows PID automatically if possible.
3. Return to the game and select the normal Quit menu. Save console output
   and installer log. The trace now follows three confirmed original OEM
   stages without touching game code: WINMAIN_RETURN reads the actual last
   MSG.wParam, CRT_WINMAIN_RESULT reads EAX after WinMain has returned,
   and CRT_EXITPROCESS_ARGUMENT reads ESI immediately before the CRT passes
   it to the imported ExitProcess. Finally, the debugger observes the real
   PROCESS_EXIT event and its actual raw status. Only then does the tracer
   exit itself. These values distinguish "WinMain returned 1" from "the CRT
   received 0 but process exited differently". Optional --messages shows
   the last twelve successfully retrieved PeekMessageA messages.

The optional --messages mode traps every PeekMessageA return, which can
reduce game performance. The default (without --messages) observes the WinMain return, the two
original CRT handoff stages and the final exit event, with much less overhead.

**Limits:** External Windows debugger / CPU hardware breakpoints might not
work under all Wine versions. This has been compiled and unit-tested in CI,
not yet executed attached to a real Matrox game. Do not run two debuggers
at once. Do not kill the tracer while the game is running: hardware
breakpoints could remain armed in game threads. The old tracer detached
immediately after WinMain and only predicted the process code; the new
trace stays attached until EXIT_PROCESS_DEBUG_EVENT and prints the observed
code. ERROR_SEM_TIMEOUT (121) from WaitForDebugEvent after 30 seconds is
normal while the game remains open, not a game error. Prefer a short run
and Quit.
This does not investigate the separate Smacker crash 0xC0000005 at
0x0046D76D tracked in issue #41.

The Windows CI tests x86 structure layout and decoding without game media,
and uploads a standalone diagnostic artifact. No proprietary data is
committed, and main is not modified by this experimental branch.
