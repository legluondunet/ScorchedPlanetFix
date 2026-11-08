# Version-commit overlay on the original root menu (PR #24)

This experiment keeps the published v2.0.0 release unchanged.

## What the next Windows/Wine build does

- The x64 launcher passes `ScorchedPlanetFix <version>-<commit>` to its embedded
  x86 GameFrame Glide renderer. The text is generated with built-in 5x7 glyphs,
  not an external font or copyrighted game assets.
- After GameFrame presents a frame, a conservative **four-region visual
  recognizer** checks the gold OPTIONS heading, blue globe, white New Game and
  white Quit panels in the original *root menu* shown by the maintainer's
  1920x1200 screenshot. The positions are normalized within the centered
  4:3 picture, so the screen resolution can vary.
- Only when all four signs are recognized does it draw light gray text at the
  **top left, without a frame**, into the final OpenGL presentation buffer.
- When the scene changes, recognition fails and the overlay disappears.
  Unknown menus and other game editions intentionally fail closed. It never
  changes the game's actual Glide framebuffer or original assets.
- `SCORCHED_MENU_PROBE` and `SCORCHED_MENU_OVERLAY` records are written to
  the same `ScorchedPlanetFix.log` for a targeted visual test.

## Important limitations

This is deliberately **not** a universal engine-state hook. The screenshot
provides visual evidence for the original root menu of the tested
EN/ES/IT edition, but does not establish that the colors and positions are
identical for every supported ISO or every video rendering path. Recognition
may require further adjustment after testing on Wine, Windows and other
editions. The build is experimental; do not merge before validation.

The title in the supplied screenshot reads `OPTIONS`, but that screen also
contains New Game, Load Game and Quit buttons: this is the targeted root menu.

## Focused acceptance test (one run)

Use the newest CI artifact from this PR. It is not an official release.

1. Launch the game normally; with an existing verified installation,
   a known older GameFrame DLL hash is backed up before upgrading.
   Unknown/proprietary DLLs are **not** overwritten.
2. Stay at the original 8-button root menu (the screen titled OPTIONS)
   for at least 8 seconds. Expect `ScorchedPlanetFix 2.0.0-<commit>` in the
   top-left area, without background or frame.
3. Open a submenu for 5 seconds, then return to root menu.
4. Start a mission for at least 8 seconds; verify the label does not
   remain visible in the gameplay or any videos.
5. Alt+Tab and return, verify display/focus stability, then quit and
   share the unified `ScorchedPlanetFix.log` plus a root-menu screenshot.

If the label does not appear, look for `SCORCHED_MENU_OVERLAY:` records;
if there are no `visible=true` transitions, the root-screen recognition
must be adjusted. If it persists outside the root menu, the experiment must
not be merged.
