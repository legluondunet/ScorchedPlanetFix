# ScorchedPlanetFix v2.0.0-rc.1 — final acceptance checklist

Test this **exact release candidate** on native Windows 11 and Wine-staging. Each tested launch must use the same RC executable and its generated `ScorchedPlanetFix.log`. Keep the published v1.1.0 untouched.

## A. Windows 11 — fresh installation + second launch

1. In a **new empty folder**, put the RC `ScorchedPlanetFix.exe` next to one supported owned ISO or CUE/BIN (do not copy any external `glide.dll`).
2. Launch without arguments. Confirm automatic extraction and SHA-256 verification, single game window, working input, correctly centered 4:3 image, no monitor resolution switch, 60 FPS maximum, videos, sound and actual gameplay.
3. Test Alt+Tab out and back; return to the game with functional input. Quit normally.
4. Run `ScorchedPlanetFix.exe --verify` and confirm read-only success.
5. Move the **disc image** away, leaving the installed `splanet/` folder. Launch again; confirm automatic verification and gameplay without any re-extraction.
6. Confirm `ScorchedPlanetFix.log` has **no false fatal error for legacy game exit code 1** (may report the return code as a diagnostic).

## B. Wine-staging — fresh installation and focus

Repeat steps A.1–A.3 and A.5 with `wine ScorchedPlanetFix.exe` in a normal Wine prefix, without a virtual desktop. Confirm the same 4:3, 60 FPS cap, functional Alt+Tab and normal return to game. Ensure 32-bit OpenGL support is installed.

## C. Optional negative tests (do not alter a working installation)

- `--help` / `--version`: shows `2.0.0-rc.1-<commit>` and no selectable nGlide backend.
- `--3dfx-logo` and `--launch`: explicitly rejected as obsolete.
- If you have a separate v1.1.0 game copy with nGlide, retain a backup first. Running v2 against its external `splanet/glide.dll` must refuse without overwriting the DLL. Move that DLL to a **backup outside `splanet/`** before using bundled GameFrame.

## Report

Send the two platform logs and only any screenshots needed for unexpected rendering. State pass/fail for Windows and Wine. The exact RC commit can be read from `--version`.

**Do not publish v2.0.0 stable until these tests are accepted.**
