# ScorchedPlanetFix

Modern compatibility tools for the original **Scorched Planet (1996)** game.

Development happens through pull requests. Original commercial game files and artwork are **not distributed** in this repository.

The first component is a Windows installer written in Rust, intended to read files copied from the original CD rather than requiring a mounted disc.

## Graphics compatibility research

The 3dfx/RenderWare investigation provides **Linux/Wine/nGlide Bash** and Windows PowerShell diagnostic tools to capture process and thread state while `SPVOODOO.EXE` is frozen at the transition from menu to a mission. The diagnostic scripts do not alter game files. Linux users can start with `tools/diagnose-3dfx-hang-linux.sh`. See [the graphics diagnostic guide](docs/graphics-diagnostics.md).

## FadeLow — user-tested 3dfx timing fix

The original game's 3dfx executable can freeze after selecting **Go** on fast CPUs. The user successfully tested **FadeLow** under Linux/Wine/nGlide. The version-locked Python patcher `tools/scorchedplanetfix_patch.py` reproduces the validated 4-byte timing adjustment **only** for the exact SHA-256 of the supported retail `SPVOODOO.EXE`.

The recommended `copy` mode keeps the original untouched. Optional `apply --yes` makes a verified backup, and `restore --yes` reverses the change. See [FadeLow installation and rollback instructions](docs/fadelow-patch.md). No modified copyrighted executable is distributed, and this development PR is not merged.

## Architecture and roadmap

The approved approach is **a version-locked, reversible EXE patcher first, with an optional 32-bit compatibility DLL only for features that need runtime hooks**. We are not rewriting the game or replacing DirectSound/RenderWare without a demonstrated need. Next investigations: original-CD dependency, internal high-resolution rendering, and Alt+Tab compatibility. See the [architecture and technical roadmap](docs/architecture.md).
