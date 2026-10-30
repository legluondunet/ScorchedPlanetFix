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
