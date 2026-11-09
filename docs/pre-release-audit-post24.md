# Post-PR #24 audit — next ScorchedPlanetFix release

Audit reference: `main` at `c7986a1660f2225ac49cac082b10b51d3ef3a65a`, after PR #24; performed 2026-11-09.
This is a **release-readiness audit**, not publication approval.

## Status confirmed by source and CI

- **Windows CI: PASS.** Run [#42533083970](https://github.com/legluondunet/ScorchedPlanetFix/actions/runs/42533083970) succeeded on that main SHA. The workflow builds the pinned GameFrame Windows x86 DLL, validates 156 decorated Glide exports, runs a native x86 LoadLibrary smoke, tests/builds the embedded Windows x64 launcher, verifies its console PE, and validates the ZIP/checksum manifest.
- **Build and packaging:** Windows x64 console executable, GameFrame x86 embedded into the executable, bundled Apache-2.0 license and SHA-256 checksums. Release is not made automatically by push/PR.
- **Game assets:** recognized editions and immutable hashes are verified before normal launch; game EXE's original backup and relative-video INI are checked. The installer does not distribute commercial game assets.
- **GameFrame migration:** normal launch moves an existing *regular* `glide.dll` with a different hash to the fixed `glide.dll.bak`, replacing any prior regular backup; the staged embedded DLL must match the expected SHA-256. No new numbered/hash-suffixed backups are created. Symlinked/non-regular paths are rejected; activation failure attempts rollback. The `--verify` command remains read-only.
- **Menu signature:** a 5x7 pixel font is scaled to approximately 9 px high, colored `#980808`, anchored bottom-right, and shown when a title-plus-corner-icon recognizer detects the root menu. The PR #25 update tolerates hovered buttons and the Language, Quit and New Game modal panels, and hides the label shortly after leaving the root menu.
- **Maintainer Wine evidence:** the attached cumulative November 8 log's session `2.0.0-3fc157cc` verifies the EN/ES/IT assets, backs up `glide.dll`, installs GameFrame and launches the game; menu telemetry shows visible/hidden transitions and approximately 59.5 FPS. The maintainer approved the smaller red signature design. This log does **not** identify the exact final `3e677200` build.

## Corrections applied in this audit PR

- Corrected `--help` to describe automatic `glide.dll` backup and replacement (now a single replaced `glide.dll.bak`), and read-only verification; added a regression assertion.
- Removed a now-obsolete fixed hash allowlist used only to label old GameFrame builds in logs. Any different regular DLL follows the same hash-recorded backup path; added a regression test that a directory named `glide.dll` cannot be replaced.
- Fixed fresh-install publication ordering: the staged tree must pass the **complete installed-game SHA-256 manifest audit, executable backup check, INI check and exact GameFrame verification** before being renamed to the user-visible `splanet/`. A rejected image leaves no invalid installation directory. The new disk-media path still requires a full end-to-end runtime test.
- Improved menu recognition: keep the compact version label visible on hovered Quit and the Language, Quit and New Game popups in the original OPTIONS root menu; add score-based Rust regression checks and short dropout tolerance without enabling the label on gameplay/video scenes.
- Aligned README, installer guide and GameFrame notes; explicitly separated the behavior of published **v2.0.0** from **post-v2.0.0 development**.
- Preserved the original v2.0.0 release notes, tag and publication workflow as historical release artifacts.

## Release gates still requiring validation

1. **Final build identification:** run `--version` on the exact final candidate and retain its corresponding `ScorchedPlanetFix.log`; do not rely on a different experimental revision's log.
2. **Runtime smoke on latest Wine-staging and native Windows:** check fresh install/second launch, videos/audio/gameplay, controller or keyboard/mouse, signature staying visible in root-menu popup panels and on hovered buttons, then disappearing for gameplay/videos, Alt+Tab and exit. Also check that an old or unknown `glide.dll` is backed up and replaced, including **overwriting an existing regular `glide.dll.bak`**, without creating suffix backups.
3. **Edition matrix:** check all four supported disc profiles (US 1.08, EN/ES/IT 1.06, FR/DE 1.06 and alternate FR/DE ISO) with exact validation and at least a menu/launch smoke. The visual menu classifier is calibrated to an EN/ES/IT screenshot; other editions may fail closed.
4. **Read-only and refusal cases:** `--verify` must leave all bytes unchanged; invalid CD/game assets, symlinks, and incomplete installations must not be overwritten.
5. **Next release packaging:** choose the next version (for example, `2.0.1` if this is a patch release); update Cargo.toml/Cargo.lock, new release notes, the package's `--version` expectation, and a **new manually gated** publishing workflow for that version. The existing `publish-v2.0.0.yml` is intentionally fixed to the existing tag and should not be rerun or retargeted.
6. **Publication approval:** review final changes/CI, approve the tagged candidate and publish only on explicit maintainer instruction.

## Residual risks / limitations

- GitHub CI can validate compilation, ABI and noninteractive tests, but not the actual 1996 game's visual transitions, Alt+Tab, or the audio/video paths.
- Identifying a known edition relies on exact executable/geometry/driver fingerprints and a verified manifest; no independently authenticated whole-disc retail master exists for every original medium.
- A single `glide.dll.bak` holds the most recently displaced Glide DLL; its previous contents are intentionally lost on a new replacement. Failure restoration is best-effort if the filesystem refuses writes/renames. The log records the error; manual recovery from `.bak` may be required.
- No evidence here establishes that root-menu image colors/positions are identical across all supported editions. The recognizer intentionally hides the label when uncertain.
- The published `v2.0.0` tag predates the automatic backup and small red menu signature; release notes attached to that tag must remain historically accurate.

**Current decision: release pending**, not yet a completed multi-platform final candidate. Complete the above gates before publishing.
