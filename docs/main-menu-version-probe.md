# Main-menu version signature — diagnostic phase

Goal: display `ScorchedPlanetFix <package-version>-<8-character-commit>`
in the **main menu only**, without changing game data, gameplay images,
videos, native window focus, or 60 FPS pacing. The stable 2.0.0 release
is not modified. This branch is intentionally experimental.

## What this PR does

- Carries the launcher identity from the x64 launcher to its embedded x86
  GameFrame through the child process environment.
- Emits compact `SCORCHED_MENU_PROBE` records to the existing
  `ScorchedPlanetFix.log` from GameFrame every ~2 seconds and whenever
  the observed frame-command category changes.
- Records whether a frame contains 3D calls, only LFB calls, or neither;
  also logs the game's `grSstPassthruMode` state. These are **diagnostics**,
  not menu identification. A movie can also have no 3D primitives.
- No on-screen text is drawn yet. This is a deliberately fail-closed
  prerequisite; rendering text during gameplay would violate the design.

## One Windows or Wine observation run

Use the PR's Windows CI artifact (not the official stable release). Keep
one existing verified `splanet/` installation and its backup. This
experimental build can upgrade the **exact GameFrame DLL hash validated
during v2.0.0 Wine acceptance**, preserving it as a fingerprinted backup;
unknown/proprietary DLLs are still rejected without modification. Other
v2.0.0 builds with different DLL hashes must not be overwritten automatically.

1. Launch the game and let the intro/videos complete.
2. Stay on the **main menu** for at least 8 seconds without input.
3. Open **Options**, stay there for 8 seconds, then return to main menu
   for 8 seconds.
4. Start a mission, play for at least 8 seconds.
5. If possible, return to the menu, stay 8 seconds, quit normally.
6. Share the single `ScorchedPlanetFix.log` and note which steps you reached.
   A screenshot of the menu is useful for placement, but not mandatory.

## Acceptance before actual overlay

A signal must distinguish the menu from videos, briefing, options and
gameplay without assumptions based only on FPS, resolution or 2D activity.
Only then add an OpenGL text pass to the *presentation* framebuffer (not
the game's original framebuffer or game resources) and test at 4:3, 16:9
and Alt+Tab under Wine/Windows. If no reliable signal exists, use
a guarded exact-edition code hook rather than invent a heuristic.
