# Version-commit overlay on the original root menu (PR #24)

This feature was merged in [PR #24](https://github.com/legluondunet/ScorchedPlanetFix/pull/24) for **post-v2.0.0 development builds**. It is not included in the already published v2.0.0 tag.

## How it works

- The x64 launcher passes `ScorchedPlanetFix <version>-<commit>` to its embedded
  x86 GameFrame Glide renderer. The text is generated with built-in 5x7 glyphs,
  not an external font or copyrighted game assets.
- After GameFrame presents a frame, a conservative **four-region visual
  recognizer** checks the gold OPTIONS heading, blue globe, white New Game and
  white Quit panels in the original *root menu* shown by the maintainer's
  1920x1200 screenshot. The positions are normalized within the centered
  4:3 picture, so the screen resolution can vary.
- Only when all four signs are recognized does it draw **original menu red (#980808, RGB 152/8/8)** text at the
  **bottom right of the presentation surface, without a frame**, into the final
  OpenGL buffer. The built-in 5x7 pixel glyphs are preserved, but the on-screen
  signature is scaled to **two-thirds** of its previous dimensions (14 px -> 9 px
  high) using nearest-neighbor filtering, keeping the same bottom-right margins.
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
may require further adjustment when the final release candidate is tested on
Wine, Windows and other editions. The existing logic intentionally hides the
signature when recognition is inconclusive.

The title in the supplied screenshot reads `OPTIONS`, but that screen also
contains New Game, Load Game and Quit buttons: this is the targeted root menu.

## Focused regression test (one run)

Use the newest post-PR24 CI artifact from `main` or a follow-up release candidate.
The already published v2.0.0 release does not contain this overlay.

1. Launch the game normally. After a successful immutable-file audit, any
   existing regular `glide.dll` differing from the bundled GameFrame is
   preserved as `glide.dll.bak` (or an unused suffixed `.bak`) before installing
   the embedded DLL. This includes unrecognized external wrappers and older
   experimental builds. Existing backups are left untouched; symlinks and
   non-regular paths are rejected. Check the `GAMEFRAME:` backup entries in the log.
2. Stay at the original 8-button root menu (the screen titled OPTIONS)
   for at least 8 seconds. Expect `ScorchedPlanetFix <version>-<commit>` in the
   bottom-right area in original menu red (#980808), without background or frame.
3. Open a submenu for 5 seconds, then return to root menu.
4. Start a mission for at least 8 seconds; verify the label does not
   remain visible in the gameplay or any videos.
5. Alt+Tab and return, verify display/focus stability, then quit and
   share the unified `ScorchedPlanetFix.log` plus a root-menu screenshot.

If the label does not appear, look for `SCORCHED_MENU_OVERLAY:` records;
if there are no `visible=true` transitions, the root-screen recognition
must be adjusted. A label that persists outside the root menu blocks publication
of the next release. The original PR #24 was merged after visual maintainer
approval; see the [release-readiness checklist](pre-release-audit-post24.md) for
remaining platform/edition regression requirements.
