# Matrox native Vulkan 3D proof — stage 2 (#73)

Base: validated Wine presentation commit 09f3c9f. This branch is deliberately separate from PR #74. It does not change MSI95 or the launcher, or claim GPU 3D is already working.

## First executable target

Render synthetic opaque triangles into an offscreen integer RGB565 attachment at 640x400 and 1440x1080. Use a sampled R16_UINT source texture (CLUT predecoded on host) and R16_UINT color attachment, check format and image usage capabilities before allocation. Read back packed pixels, compare interior coverage/texture and RGB565 math against the CPU reference, separately inspect edge coverage. Vulkan top-left rule is not the CPU's epsilon rule; edge parity must be measured, not assumed.

Shaders in tools/matrox/shaders/:
- Input vertex layout 32 bytes: float xy[2], uv[2], invW, modifier[3]; offsets 0,8,16,20. Push constants 24 bytes: logical extent, target extent, flags, reserved; identical stages. Positive viewport height; y=0 at top. CPU pre-transformed x/y stay native; high-resolution viewport changes rasterization density, not game coordinates.
- Explicit noperspective UV*invW and invW, affine RGB modifiers. Host must reject non-finite coordinates and invalid invW; for affine draws use invW=1. No depth testing: existing CPU path is ordered painter-style.
- Integer nearest/bilinear using RGB565 channels and 8-bit weights, then legacy /255 modulation and optional 4x4 dither at logical native coordinates. The first proof excludes all transparency/keying, indexed exceptions, opacity below 100, untextured shadows, RGB888 lighting, sparse atlas, and Z-planes. Do not send live draws yet: many world textures have Transparency=1 and would not qualify even if visually opaque. GPU capture shader compile/validation alone is not pixel parity.

## Integration contract and blockers

The guest directly writes color/depth/texture pointers, and msiSetParameters(NULL) rebinds color/depth addresses after 0xDEADBEEF unlock. The -1 sentinel is not a render state. At a CPU access barrier, pending GPU writes must be completed and native readback synchronized before returning the pointer. CPU writes must be uploaded before later GPU draws. Track texture contents at draw time (not just cache offsets), and retain ordering across untextured triangles, shadow-underlay/once rules, and Z-planes. For actual game use, avoid readback per triangle: measure consecutive eligible runs and barrier frequency. Preserve native 640x400 backing buffer for guest reads and F12; a separate high-resolution target cannot replace this native allocation. Full high-resolution compositing with HUD requires explicit ordering and coverage; no naive overlay of the final 640x400 framebuffer. Depth testing is not automatically enabled in the current CPU rasterizer. No changes to default Renderer, no runtime compiler dependency, no new user-facing test at this shader-only stage.

CI compiles GLSL to SPIR-V for Vulkan 1.0 and validates it; compiler package is a CI-only tool, no shipped binary. Next: standalone offscreen readback harness using the validated shader pair and compare native/scaled reference pixels; only then wire up runtime GPU rendering.
