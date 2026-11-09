# Version-commit overlay on the original root menu (PR #24)

This feature was merged in [PR #24](https://github.com/legluondunet/ScorchedPlanetFix/pull/24) for **post-v2.0.0 development builds**. It is not included in the already published v2.0.0 tag.

## How it works

- The x64 launcher passes `ScorchedPlanetFix <version>-<commit>` to its embedded
  x86 GameFrame Glide renderer. The text is generated with built-in 5x7 glyphs,
  not an external font or copyrighted game assets.
- After GameFrame presents a frame, a conservative **menu-title and corner-icon visual
  recognizer** checks the gold OPTIONS heading plus the globe, New Game and Quit
  icons in the original *root menu*. The heading stays visible when hovering
  buttons or opening in-menu popup panels; individual corner icons may be
  covered or dimmed. Entering the menu requires the title and two of those
  icons, while staying there requires the title and at least one. A maximum
  three-frame detection dropout prevents flicker during hover transitions.
  The sampling coordinates are normalized to the centered original picture,
  so the resolution can vary.
- Only when all four signs are recognized does it draw **original menu red (#980808, RGB 152/8/8)** text at the
  **bottom right of the presentation surface, without a frame**, into the final
  OpenGL buffer. The built-in 5x7 pixel glyphs are preserved, but the on-screen
  signature is scaled to **two-thirds** of its previous dimensions (14 px -> 9 px
  high) using nearest-neighbor filtering, keeping the same bottom-right margins.
- While the original root menu is visible, the signature remains visible
  above its **Language, Quit confirmation and New Game popup panels**, and when
  hovering Quit. When leaving for a video or mission, recognition fails and
  the overlay disappears after at most four consecutive frames without menu
  evidence. Unknown scenes intentionally fail closed. It never changes the
  game's actual Glide framebuffer or original assets.
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
   moved to `glide.dll.bak`, replacing any existing regular backup, before
   installing the embedded DLL. This includes unrecognized external wrappers
   and older experimental builds. No new suffixed backups are created;
   symlinks and non-regular paths are rejected. Check the `GAMEFRAME:` entries in the log.
2. Stay at the original 8-button root menu (the screen titled OPTIONS)
   for at least 8 seconds. Expect `ScorchedPlanetFix <version>-<commit>` in the
   bottom-right area in original menu red (#980808), without background or frame.
3. Hover the Quit button and open the Language, Quit and New Game popup
   panels, each for at least five seconds. Expect the label to **remain
   visible** in all four states, without blinking.
4. Start a mission for at least 8 seconds; verify the label **disappears**
   and does not remain visible in gameplay or any videos.
5. Alt+Tab and return, verify display/focus stability, then quit and
   share the unified `ScorchedPlanetFix.log` plus a root-menu screenshot.

If the label does not appear or disappears in a popup, look for
`SCORCHED_MENU_OVERLAY:` score and transition records. A label that persists
outside the root menu blocks publication
of the next release. The original PR #24 was merged after visual maintainer
approval; see the [release-readiness checklist](pre-release-audit-post24.md) for
remaining platform/edition regression requirements.
