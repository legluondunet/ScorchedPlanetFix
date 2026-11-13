# Pre-release code audit — 2026-11-13

> Scope: `main` as of `f06b1c49d874f90839f8c367624be23b9ec3d0d8`. This audit deliberately **does not publish a release** or change the game's verified binaries, gameplay, supported CD editions, or GameFrame runtime.

## Removed in this cleanup

| Component | Reason | Replacement |
| --- | --- | --- |
| `src/iso_install.rs::install_iso` | Unused ISO-only wrapper | The installed CLI already calls `install_image`, which supports ISO and CUE/BIN |
| `src/verify.rs::launch_verified` | Unused entry point carrying `#[allow(dead_code)]` | Production already uses `verify_game_assets` → `launch_preverified` |
| `src/cue_image.rs::normalize_cue` | Unused convenience wrapper only exercised by its own unit test | Test now exercises the production `normalize_cue_with_bin` entry point |
| `src/gameframe.rs::is_embedded_gameframe` | Redundant test-only runtime helper | The unit test already checks the DLL's SHA-256 and calls `verify_local_gameframe` |
| `InstallReport.folder` | Return field never read by the installer | Retain reported edition, count, bytes and patch result |
| Unused imports and struct `Guid` | Rust compiler warns that they are never referenced | Keep all used FFI declarations and working diagnostic functions |
| `publish-v1.0.0.yml`, `publish-v1.1.0.yml` | Historical, version-locked publication jobs for existing releases | GitHub release assets, tags, and historical release notes remain |
| `publish-v2.0.0.yml`, `publish-v2.0.1.yml` | Historical publication jobs fixed to existing versions and tags | A **new** manually gated workflow must be reviewed for the next version |
| `prune-non-main-once.yml` | Already-executed one-shot branch-pruning job, guarded by a literal old merge-message marker | No automatic branch deletion in normal development |

The active `rust-fadelow-patcher.yml` Windows CI workflow and manual `cleanup-windows-ci.yml` workflow remain. The Windows CI action dependencies are updated to Node 24 variants (`actions/checkout@v5`, `actions/upload-artifact@v6`) without changing its build/test/package pipeline.

## Validation of the cleanup branch

- **Windows CI passed:** [run #43446264188](https://github.com/legluondunet/ScorchedPlanetFix/actions/runs/43446264188) — x86 GameFrame library and 156-export smoke, Rust x64 launcher tests, package verification and artifact upload.
- The previous Node 20 deprecation warning is absent after upgrading the two GitHub Actions.
- Remaining warnings stem from the pinned upstream GameFrame workspace/linker and the launcher `Logger.path` field (retained for pending PR #32). None of the removed Rust helpers or deleted fields is referenced by the tested build.

## Retained intentionally

- **Installer trust boundary:** edition-specific executable hashes, CD inventory limits and path validation, staged installation, preservation of original `.bak` backups, refusal to overwrite unknown installations.
- **FadeLow/No-CD migration:** `src/lib.rs` still recognizes historical four-byte FadeLow executables and validated older backup names to safely migrate owner-supported editions. These are compatibility paths, not dead code.
- **GameFrame safety:** embedded x86 DLL hash and PE checks, protected single `glide.dll.bak` update, window and 60-FPS behavior; the next feature PRs must preserve these in ordinary launch mode.
- **Historical release notes and test documents:** kept for traceability; they do not authorize republishing old versions.
- The current `Logger.path` is not read in production but **is referenced by the open PR #32 logger test**; it is intentionally kept until the two logging branches are integrated. Public `make_fadelow` and `Action::{Status,Copy,Restore}` library API surfaces are not called by the current CLI, but could be referenced by external consumers/tests. Their removal needs a separate, explicit API-compatibility decision.

## Pending integrations and regressions

- [ ] **PR #32**: split installer log (`ScorchedPlanetFix_installer.log`) from GameFrame DLL log (`ScorchedPlanetFix.log`), correlate sessions; rebase/merge with other pending work, then verify on Wine/Windows.
- [ ] **PR #33**: opt-in `--launch-no-verify` direct launch without file checks; preserve strict default launch and `--verify`; test under Wine/Windows.
- [ ] **Diagnostics review:** launcher currently forces `SCORCHED_MENU_PROBE=1`, originally introduced as temporary root-menu telemetry. Evaluate opting in only for troubleshooting after PRs #32/#33 integrate; do not delete menu-recognition code or dark-zone diagnostics prematurely.
- [ ] Review warnings from the pinned upstream GameFrame build separately; do not patch the third-party API or delete decorated exports solely to suppress linker diagnostics.
- [ ] CI on the **combined** release candidate: Rust x64 launcher tests, pinned GameFrame x86 ABI and 156-exports test, package inspection, no-media and unchecked smoke, read-only `--verify`.
- [ ] End-to-end Wine/Windows smoke: known supported US/EN-ES-IT/FR-DE disc image(s), first install, second launch, Alt+Tab, videos, menu signature, game exit, and comparison of the two log files.
- [ ] Decide next version/tag, update `Cargo.toml`/`Cargo.lock` and versioned release notes, and create a new *manual-only* publishing workflow. **Do not repoint any historical workflow or overwrite existing release tags.**
- [ ] Confirm the known deferred dark-region/transparency issue is either documented as a limitation or fixed before publishing.

This report distinguishes verified unused code from potentially valuable backwards-compatibility code. It does not claim that CI alone proves gameplay compatibility.
