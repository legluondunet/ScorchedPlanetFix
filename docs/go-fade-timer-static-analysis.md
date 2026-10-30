# Static Go-fade timing investigation (2026-10-30)

Analyzed the user-supplied original `SPVOODOO.EXE` (PE32/i386, SHA-256
`fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a`)
offline. No Wine process was launched and the executable was not modified.

## Verified instructions

- `0x434D1C`: calls imported `QueryPerformanceFrequency`.
- `0x434D5F`: multiplies the returned frequency by the double constant
  at `0x4B48F0`, which is exactly **1/12**.
- `0x434D65`: stores the scaled frequency as float at `0x57BC58`.
- `0x432B2F`: calls `QueryPerformanceCounter`, overwriting the last
  counter value before computing the difference.
- `0x432B8A-0x432B90`: divides that difference by scaled frequency
  and writes the float delta at `0x556314`.
- `0x4495C6`/`0x4495D4`: if delta is at or below float `0x38D1B717`
  (0.0001 game units), skips color interpolation and countdown decrement.
- `0x4496EE`: decrements remaining fade duration when delta passes the gate.
- `0x4496F4`: checks the remaining time, and the fade returns nonzero
  while continuation is needed. A zero return occurs at `0x449716`.
- `0x427D24-0x427D38` is one caller that repeats on a nonzero return;
  other similar palette-fade loops also exist.

## Specific possible starvation mechanism

The high-resolution timer's effective time scale is 12 game units per
real second. The 0.0001-unit minimum delta corresponds to about **8.33 us**.
Because each call overwrites the baseline counter even if the fade drops
the delta, successive loop iterations below 8.33 us can yield zero
countdown progress forever. This could plausibly explain a fast-CPU
busy loop without a window update. Static code proves the possibility,
**not that it is the unique cause of the reported Go freeze**.

A deterministic 10-MHz-QPC, float32 gate model confirmed the behavior:
at fixed 1, 5 or 8 us per iteration, 300,000 iterations made no
progress; at fixed 9 or 20 us, the model completed the transition.
This is a toy test of branch semantics, not actual runtime evidence.

IMPORTANT: The fade argument `3.0` represents about 0.25 wall seconds
on this scaled QPC branch, **not three seconds** as previously assumed.
Other branches, including a fallback clock, may use a different scale.

## Remaining uncertainties

- The fade loop actually executed at the moment the Go button froze
  has not been located unambiguously. The sampled `0x427D38` could
  belong to a different transition, e.g. cleanup.
- Live readings of `0x556314` (delta) and `0x4C99C8` (remaining fade
  duration) would confirm or falsify subthreshold starvation.
- Repeating audio is consistent with halted game updates while sound
  buffers continue playing, but is not proof of an audio or timer cause.
- Do not patch game or graphics DLLs based solely on static evidence.

## Next

Use opt-in, read-only runtime observation on the existing Wine prefix
to verify actual timer/fade values during the Go freeze. If confirmed,
an experimental compatibility fix should accumulate discarded deltas
rather than abruptly skipping the fade or changing global timers.
Guard any eventual patch by exact executable hash, preserve original,
provide easy rollback, and keep the research PR in draft until validated.

## Extended offline cross-reference inventory

A full `objdump -d -M intel` pass over the original executable found
**35 direct calls** to the shared fade-update function (`0x4495C0`)
and **18 direct calls** to its setup routine (`0x449450`).
Among the 35 updates, these **12 repeating wait edges** are verified
at instruction boundaries, each encoded as `75 ea` (`jne -22`):

| Conditional branch | Target (repeated timer/fade call) |
| --- | --- |
| `0x4193D0` | `0x4193BC` |
| `0x4195F5` | `0x4195E1` |
| `0x419DC6` | `0x419DB2` |
| `0x419EBC` | `0x419EA8` |
| `0x41FED9` | `0x41FEC5` |
| `0x426DB1` | `0x426D9D` |
| `0x427D38` | `0x427D24` |
| `0x430602` | `0x4305EE` |
| `0x43067F` | `0x43066B` |
| `0x456BB9` | `0x456BA5` |
| `0x457B37` | `0x457B23` |
| `0x4586A7` | `0x458693` |

These loops share one fade-update function, suggesting one narrowly
scoped fix would cover several transitions, **if** the live failure
is indeed caused by the minimum-delta gate.

Many call sites check a runtime value at `0x578128` against `8`
before enabling a palette fade. Its setup code passes the address
to a graphics initialization helper. We have not established
the active value during the actual 3dfx freeze.

## Closer correlation with the recorded live CPU sample

Of the **178** previous `perf` user-mode CPU samples, exactly
**26 instruction pointers** fell within the mapped original EXE's
`.text` address range. Of those 26, **20** are in timer
`0x432B00`, **5** in `0x436100`, and **1** on the
loop branch `0x427D38`. The other two `[JIT]`-labeled
IP samples are outside the original EXE image and should not
be automatically attributed to the retail game's code.
This is meaningful support for timing/polling as a hotspot,
but sampling only the main thread still cannot identify every
caller nor rule out a graphics/audio dependency.

## Intermittent-jitter scenario

To illustrate why occasional long iterations need not rescue the
animation promptly, consider a deterministic model with normal
5-microsecond iterations (below the gate) and occasional
100-microsecond iterations (above the gate). Only the
100-microsecond iterations decrement the fade countdown.

| Frequency of a 100-us iteration | Approximate fade completion |
| --- | ---: |
| One per 100 iterations | 1.5 s |
| One per 1,000 iterations | 12.7 s |
| One per 10,000 iterations | 125 s |
| One per 100,000 iterations | 1,250 s |

These values are conditional model outputs **not observed
timings on the user's CPU**. They assume a fixed 10 MHz QPC
frequency, the actual divide-by-12 scaling, a 3-unit fade,
and recurring 5/100 us loop intervals.

## Proposed narrowly scoped proof of concept (NOT APPLIED)

The game-specific gate is embedded as IEEE-754 float immediate
`0x38D1B717` at virtual address `0x4495CD`, corresponding
to **raw PE file offset `0x489CD`** in the exact verified
original executable. A prospective experiment could replace
**only** that threshold with zero, so positive deltas are no
longer discarded. This is a four-byte change in the
**fade function**, not the global timer API, and might
allow normal countdown progress.

**Caution:** every positive elapsed-time increment would then
run the full 256-entry palette update rather than skipping very
small deltas. This could alter frame pacing or CPU cost.
An alternative is to accumulate subthreshold deltas until
the threshold is crossed; this preserves update frequency but
requires stateful instrumentation and is more invasive.
Neither approach has been applied, shipped, or validated.

Any later opt-in experimental build must check the exact SHA-256,
make a new output copy, preserve and verify the original,
offer rollback, and be tested under Wine/nGlide before release.

## Reproducible offline evidence

`tools/verify_go_fade_static.py` checks the original SHA-256,
PE section layout, 7 critical machine-code signatures,
the 12 back-edge signatures, the binary's 1/12 constant
and both float32 thresholds. Its `--self-test` mode uses
deterministic clock models without copyrighted binaries
and runs in GitHub Actions.

## State-dispatch route containing the sampled fade branch

The executable has a state-specific dispatcher at `0x436190`.
It reads `0x537EB0`, bounds-checks the value 0..4, and jumps
through the table at `0x4361BC`. Raw table words decode to:

| State value | Dispatch target | Next destination |
| --- | --- | --- |
| 0 | `0x4361A6` | `0x427C80` |
| 1 | `0x4361BA` | returns |
| 2 | `0x4361AB` | `0x41FF70` |
| 3 | `0x4361B0` | `0x456E20` |
| 4 | `0x4361B5` | `0x403E80` |

The sampled palette wait branch at `0x427D38` lies inside
the state-0 initialization/cleanup path beginning `0x427C80`.
`0x436190` is called by several menu/state transitions
and state writes also occur outside the dispatcher.
We therefore have a tighter **possible control-flow path**
for the observed CPU sample, but no proof which state the
game selected when the user clicked Go.

Future runtime observation of the value at `0x537EB0` would
help distinguish the menu-to-game transition from other
fade and cleanup code without changing any game assets.

## Copy-only experimental builder (created, NOT validated in game)

`tools/experimental_go_fade_patch.py` now implements a strictly
opt-in, version-gated build of **two independent test copies**.
It does not modify the user's installed executable, and
no patched commercial binary is published to GitHub.

- `low`: replaces the float32 immediate at VA `0x4495CD`
  (raw PE offset `0x489CD`) with `1e-5` game units (approximately
  0.833 us in the /12 QPC timer branch).
- `zero`: replaces the same immediate with `0`, allowing any
  strictly positive floating-point time step to proceed.
- In both cases all other bytes are unchanged; instruction lengths
  and branch targets are preserved.

Example **only for isolated offline generation**, not a recommendation
to overwrite an installed game:

```bash
python3 tools/experimental_go_fade_patch.py /path/to/original/SPVOODOO.EXE \
  --output /path/to/isolated-test/SPVOODOO.EXE \
  --mode low --i-accept-the-risk
```

The input must match the original SHA-256 and expected instruction
signature; output must not exist (create-only `O_EXCL`), may not
be the source path, and the command requires an explicit risk flag.
Generated test copies are not shipped, installed, or game-tested.

**Local verification against the user's original file:**

| Mode | Output SHA-256 (local reproducibility) |
| --- | --- |
| low | `0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45` |
| zero | `274fb66521339e0c36567293bcd26899d6b868f2c18cc897e6b1bbbabd969c9c` |

Both generated PE copies retain their size (836,608 bytes),
pass `objdump` PE-i386 decoding, and differ from the original
**at exactly the four-byte threshold immediate**.
Six synthetic PE unit tests run locally, including safe rejection
of unknown hashes and forbidden overwrites. The same synthetic
fixture tests run in Linux CI without access to the commercial game.

**Important safety/compatibility caveat:** this test changes how
often a 256-color interpolation is calculated, not merely the
clock. An extremely small threshold could cause excessive CPU
work or reveal unrelated timing assumptions. Without a runtime
test confirming successful level transition and audio/graphics
behavior, this is an experiment, not a confirmed fix.

## Future isolated Wine/nGlide validation checklist (not yet performed)

**Status:** the offline static analysis and copy generation passed;
**no Windows/Wine gameplay validation has been run**.

If a later manual game test is authorized, compare unchanged
retail `SPVOODOO.EXE` against the `low` experiment first.
Keep the game executable and data in an **isolated duplicate**
of the installation folder, not the existing Lutris game folder.
Use the same Wine runner, Wine prefix, graphics wrapper and
display settings so that only the four-byte fade constant changes.
Do not run the game itself as root; sudo is not required for
creating or launching the experimental copy.

Record the following outcomes:

1. Main menu loads and plays music as before.
2. Clicking Go proceeds to gameplay instead of freezing;
   measure the time and note whether the last ~4 s of audio repeat.
3. Loading an existing saved game works.
4. Menu fade transitions, fullscreen/focus, and 3dfx visuals are intact.
5. CPU usage and, if necessary, a new perf capture identify
   whether the busy main-thread timing loop disappears.
6. Quit and restart several times to check repeatability.

If the `low` copy behaves identically to the original, do not
claim success; consider the `zero` experiment only with a new
explicitly isolated copy and the same observations. If either
causes a new problem, stop using the test copy. Rollback consists
of deleting the isolated copy; the original remains untouched.

Separately, the read-only live-value test of timer delta
`0x556314`, fade remaining `0x4C99C8`, and game-state
`0x537EB0` would distinguish timer starvation from
other causes when the user is ready for in-game diagnostics.
No live-memory capture is performed by the offline tools.

