# ScorchedPlanetFix

Modern compatibility tools for the original **Scorched Planet (1996)** game.

Development happens through pull requests. Original commercial game files and artwork are **not distributed** in this repository.

The first component is a Windows installer written in Rust, intended to read files copied from the original CD rather than requiring a mounted disc.

## Graphics compatibility research

The 3dfx/RenderWare investigation provides **Linux/Wine/nGlide Bash** and Windows PowerShell diagnostic tools to capture process and thread state while `SPVOODOO.EXE` is frozen at the transition from menu to a mission. They do not patch game files. Linux users should start with `tools/diagnose-3dfx-hang-linux.sh`. See [the graphics diagnostic guide](docs/graphics-diagnostics.md).
