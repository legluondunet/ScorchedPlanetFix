# Matrox OEM original WinMain exit-code investigation

## Result: original exit code 1 explained (Windows native, 23 November 2026)

The original 1996 Matrox Mystique OEM `SPLANETW.EXE` may return **0 or 1
after a normal menu Quit**. Hardware-register-only tracing of its *unmodified*
original executable proves why: `WinMain` returns the **`wParam` of its
last retrieved Windows `MSG`**, not a dedicated success/error return status.

**Confirmed session returning 1** (native Windows, PID 32900, TID 34720):

```text
WINMAIN_RETURN pid=32900 tid=34720 at 0x00416cec current quit_flag=Some(1)
MSG message=0x0201 WM_LBUTTONDOWN hwnd=0x000905ac wParam=0x00000001 (1) lParam=0x00df0261 time=342873125 pt=(609, 223)
PREDICTED_CRT_EXIT_CODE=1 source=WinMain_MSG_wParam_at_ESP_plus_0x24
CRT_WINMAIN_RESULT tid=34720 eax=1 (0x00000001); original CRT about to push WinMain return
CRT_EXITPROCESS_ARGUMENT tid=34720 esi=1 (0x00000001) before push esi / imported ExitProcess
PROCESS_EXIT raw_code=1 (0x00000001) captured_winmain=true crt_result_seen=true crt_exit_seen=true
```

The `WM_LBUTTONDOWN` message uses `wParam=0x0001` because the left mouse
button is down (`MK_LBUTTON`). The original `WinMain` returned this value,
the CRT passed it unchanged to `ExitProcess`, and Windows reported
`PROCESS_EXIT raw_code=1`. This is **not**, in that session, a renderer
failure, Wine-specific fault or launcher-invented error code.

**Confirmed contrasting session returning 0** (native Windows, PID 34460):

```text
WINMAIN_RETURN pid=34460 tid=16196 at 0x00416cec current quit_flag=Some(1)
MSG message=0x0200 WM_MOUSEMOVE hwnd=0x00680a84 wParam=0x00000000 (0)
CRT_WINMAIN_RESULT tid=16196 eax=0 (0x00000000)
CRT_EXITPROCESS_ARGUMENT tid=16196 esi=0 (0x00000000)
PROCESS_EXIT raw_code=0 (0x00000000) captured_winmain=true crt_result_seen=true crt_exit_seen=true
```

The matching launcher log independently reported game PID 34460 and
`CLI: The game exited with code 0.`

**Project policy (owner-approved):** Do not patch or distribute modified
original `SPLANETW.EXE`; do not artificially force its exit code to 0.
For the Matrox OEM edition, both 0 and 1 are accepted *as possible normal
menu-exit statuses*, while the actual value remains visible in the log.
If a separate crash or abnormal shutdown is observed, it must still be
investigated: accepting status 1 is **not** a universal proof of success.
Access violation `0xC0000005` (Smacker issue #41), unknown exit codes and
retail 3dfx behavior remain independently classified.

Launcher reporting PR: #43. Diagnostic trace PR: #52. This separate
experiment is not bundled in the normal installer.

## Authentic OEM static analysis

From the owner's original 840,704-byte Matrox OEM `SPLANETW.EXE`, PE32,
ImageBase `0x00400000`, SHA-256:
`1f2684205f4aa349183acfef6eb660e92e6bb24732e5c56278f0df1111108894`.
The proprietary EXE is never committed or redistributed.

- `0x00416A73`: calls `PeekMessageA` with a local `MSG` at `[esp+0x1C]`.
- `0x00416A79`: tests `PeekMessageA` result to determine if a message exists.
- `0x00416A7D`: reads `MSG.message` from `[esp+0x20]`.
- `0x00416A84`: branches when it sees `WM_QUIT` (`0x12`).
- `0x00416CB6`: reads the game's internal quit flag at `0x00691B28`,
  which also permits exiting without having received `WM_QUIT`.
- `0x00416CEC`: executes `mov eax,[esp+0x24]`, which is `MSG.wParam`
  because `MSG` begins at `esp+0x1C` and `wParam` is at offset +8.
- `0x00416CFA`: returns EAX from `WinMain`; original CRT startup calls
  `WinMain` at `0x004A712B` and uses its result at `0x004A7130`.
- `0x004AC416`: original CRT passes the stored exit value via ESI to the
  imported `ExitProcess`. Both values and the final exit event were
  verified by live, external, hardware-breakpoint tracing.

## Optional standalone trace reproduction

`tools/matrox/trace_oem_exit.rs` builds the independent, native Windows
**x86** `MatroxExitTrace.exe` debugger. It is not part of the launcher.
It validates original OEM instruction signatures and uses CPU debug registers
(DR0/DR1) rather than editing game code, data, IAT, original DLL or on-disk
assets. It observes `WinMain`, CRT handoff and `EXIT_PROCESS_DEBUG_EVENT`.

1. Launch the original Matrox edition normally and leave it in its menu.
2. Run `MatroxExitTrace.exe` in another Windows terminal (or same Wine
   prefix if applicable). It auto-detects exactly one signature-verified
   `SPLANETW.EXE` **Windows PID**. A Linux/Unix host PID under Wine is not
   a Windows PID; an incorrect PID can yield Win32 error 87.
3. Exit the game via its normal menu and retain the tracer console output
   and the matching `ScorchedPlanetFix_installer.log`.
4. To see up to twelve recent retrieved Windows messages, use optional
   `MatroxExitTrace.exe --messages` (slower, more intrusive).
   The default traces only the WinMain / CRT / ExitProcess stages.

`WAITING_FOR_GAME_QUIT debugger wait timeout is normal while game runs.`
is informational; a 30-second debugger wait timeout is not a game crash.
The tool was built and unit-tested by Windows x86 CI and **successfully
attached to the real Matrox OEM game on native Windows**. A live Wine
debug-attach test has not separately been established. Do not attach a
second debugger or forcibly interrupt the tracing process while debug
registers are active; finish by quitting normally.

No proprietary game assets are committed. The main branch is unaffected
until the diagnostic PR is explicitly merged.
