# ScorchedPlanetFix v2.0.0 — Open-source GameFrame Glide

**Release candidate:** v2.0.0-rc.1. These notes describe the intended stable v2.0.0 release. The published v1.1.0 tag and assets remain unchanged until a separate, explicitly authorized release.

## Major new capabilities

- **Integrated open-source GameFrame Glide 2.11 → OpenGL**. Its pinned, Apache-2.0 Windows x86 DLL is embedded inside the Windows x64 launcher and installed automatically as `splanet/glide.dll`. No additional Glide download is needed.
- **Automatic first run:** identify a supported ISO or CUE/BIN beside the launcher, extract into a temporary directory, patch the original game, install GameFrame, verify hashes and start the game.
- **Automatic subsequent runs:** locate the existing game, SHA-256-check the executable, original backup, immutable assets and exact GameFrame DLL, then launch without re-extraction or an original CD.
- **Modern desktop display:** one borderless game/input window, original 4:3 picture scaled and centered within the current desktop without requesting a video-mode/refresh-rate switch; Glide buffer swaps capped at 60 FPS.
- `--install-only` and read-only `--verify`; one `ScorchedPlanetFix.log` with hardware, installation and rendering diagnostics.

## Breaking changes (v1.x → v2)

- nGlide and other external Glide implementations are **no longer supported**. All nGlide detection, per-process settings, fallback/copy logic and optional splash support have been removed.
- The `--3dfx-logo` and legacy `--launch` switches are removed.
- If a 1.x installation contains nGlide at `splanet/glide.dll`, **manually back up/move that DLL outside the game directory**. The v2 launcher will not overwrite it and refuses to launch with an unsupported wrapper. Once removed, GameFrame is installed only after the original game assets pass verification.
- Exact known earlier GameFrame experimental DLLs are eligible for safe automatic upgrade with a hash-labelled backup.

## Preserved compatibility

- Supported original game editions: US 1.08; Europe EN/ES/IT 1.06; Europe FR/DE 1.06 and its alternate ISO.
- Edition-specific No-CD and FadeLow fixes, game saves, portable video INI and hash-checked original executable backups.
- Windows 11 and Linux with Wine-staging / GE-Proton (previous combined GameFrame branch validated by maintainer).
- Strict refusal of unknown discs, modified game data and unsupported DLLs. No original game or proprietary Glide drivers are distributed.

## Release candidate acceptance

The final v2-only RC must pass Windows CI, compiled Glide ABI/LoadLibrary checks, release ZIP hashes, a complete first/second-launch test on Windows and a gameplay/Alt+Tab test under Wine. Report issues with the single `ScorchedPlanetFix.log`.

**Licensing:** ScorchedPlanetFix GPL-3.0-or-later; embedded GameFrame Apache-2.0. Unaffiliated with Criterion or other rights holders.

Maintainer: legluondunet assisted by ChatGPT (OpenAI).
