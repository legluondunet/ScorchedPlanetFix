# Matrox Mystique MSI95 clean-room driver (v0.35)

This directory contains the **independently implemented, open-source**
MSI95 Win95 compatibility layer used by the experimental Matrox OEM backend.
It is not the original Matrox proprietary `msi95.dll`.

The source is currently retained as a deterministic gzip+base64 encoding in
`msi95_probe.c.gz.b64` to keep the v0.35 experiment byte-for-byte reproducible.
It decodes to 116,216 bytes of C source, with SHA-256:

`a1a4291540de851305977fde576df57d42c64600f5abbcc1a2e1c0469d98d666`

To inspect the plain C code on Linux:

```bash
base64 -d tools/matrox/msi95_probe.c.gz.b64 | gzip -dc > /tmp/msi95_probe.c
sha256sum /tmp/msi95_probe.c
```

The Windows CI executes `tools/matrox/build-msi95.ps1` which restores the
exact source, confirms its hash, and builds a freestanding x86 PE32 DLL
without the original Matrox SDK or proprietary binaries. The nine MSI95
exports and PE architecture are checked by `audit_exports.py`, and the
resulting DLL is embedded with GameFrame in the Windows x64 Rust launcher,
using a build-time SHA-256. The running launcher verifies that hash again
before installing the DLL into its isolated Matrox runtime.

This is still a diagnostic-origin renderer: its default logging and
framebuffer BMP capture must be streamlined before a stable v2.1.0 release.
