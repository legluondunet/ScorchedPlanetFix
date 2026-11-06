ScorchedPlanetFix — GameFrame Glide experiment
================================================

NOT AN OFFICIAL RELEASE. This Windows x64 test artifact is built from
ScorchedPlanetFix PR #21, dependent on the automatic-launch work in PR #20.
It is not ScorchedPlanetFix v1.1.0. No original game binaries are included.

The single ScorchedPlanetFix.exe embeds a 32-bit GameFrame Glide DLL built
reproducibly from the open-source Apache-2.0 repository:
https://github.com/tanepiper/game-frame
Pinned source commit: f195a074a63cc150673c38becdec696163b0b3d1
The EXE also contains ScorchedPlanetFix under GPL-3.0-or-later.
Full Apache license included as LICENSE-APACHE.txt.

HOW TO TEST (WINE / PROTON)
--------------------------
Start with a new directory containing the downloaded ScorchedPlanetFix.exe
and your own supported original ISO or CUE/BIN disc image. Do NOT copy
proprietary nGlide glide.dll into the folder for this test.

  wine ScorchedPlanetFix.exe

The app detects the media, extracts and patches the supported edition,
copies the SHA-256-verified bundled GameFrame DLL to splanet/glide.dll,
verifies all original game assets, then automatically launches the game.

The game can use Wine's OpenGL implementation. Ensure 32-bit OpenGL drivers
are installed on the Linux host. The initial compatibility experiment uses
Glide 640x480 at 2x output (1280x960), 4:3; it does NOT yet claim seamless
desktop fullscreen scaling or universal game compatibility.

Run ScorchedPlanetFix.exe a second time after removing/moving your original
image file. The game should be detected from splanet/, verified and launched
again. Compare menus, 3D geometry, textures, colors, 2D framebuffer, Smacker
videos, performance, focus, input and gameplay with the known-working nGlide
baseline.

The single ScorchedPlanetFix.log beside the launcher includes GameFrame's
native Glide diagnostics (GameFrame writes into this file in append mode).
Send the log plus screenshots of any rendering defects.

SAFE FALLBACKS
--------------
- A pre-existing glide.dll is not overwritten. Such a DLL continues to run
  with the prior nGlide-compatible per-process configuration. You may keep
  using nGlide as a known-working baseline.
- A fresh GameFrame DLL is only installed on an existing splanet directory
  after read-only SHA-256 checks confirm its patched executable, original
  backup and all immutable game files.
- Broken/unrecognized game installations do not get patched or overwritten
  automatically.
- The optional --3dfx-logo requires the proprietary nGlide implementation
  and your own 3DfxSpl.dll; GameFrame cannot use that nGlide-specific option.
- --verify is read-only; --install-only explicitly performs safe migrations.
- The original game, nGlide and any proprietary driver are NOT bundled.
- There are no modifications to ScorchedPlanetFix v1.1.0 release or main.

VALIDATION LIMITS
-----------------
CI checks that the GameFrame DLL is a PE32 x86 binary with exactly the
156 upstream decorated Glide exports, runs a native Win32 LoadLibrary
smoke test, and runs the launcher unit tests and packaging audit. CI
cannot prove all 45 Scorched Planet RWDLVOOD.DLL imports or the 3D rendering
without an original user-supplied binary and real game session.
The game rendering and Windows/Wine user tests are still REQUIRED.

Credits: legluondunet assisted by ChatGPT (OpenAI)
