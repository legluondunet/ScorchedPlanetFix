#!/usr/bin/env python3
"""Fail-closed MSI95 x86 ABI/export validation. Uses pefile from CI."""
import argparse
import pathlib
import pefile

EXPECTED = {
    "msiAllocTextureHeap", "msiEndFrame", "msiExit",
    "msiFreeTextureHeap", "msiInit", "msiRenderTriangle",
    "msiRenderZPlane", "msiSetParameters", "msiStartFrame",
}

def audit(path: pathlib.Path) -> None:
    image = pefile.PE(str(path), fast_load=False)
    if image.FILE_HEADER.Machine != 0x014C:
        raise SystemExit("MSI95 DLL must be Windows x86 PE32")
    if image.OPTIONAL_HEADER.Magic != 0x010B:
        raise SystemExit("MSI95 DLL must use PE32 optional header")
    if not image.FILE_HEADER.Characteristics & 0x2000:
        raise SystemExit("MSI95 output is not marked as a DLL")
    if not hasattr(image, "DIRECTORY_ENTRY_EXPORT"):
        raise SystemExit("MSI95 DLL has no export directory")
    exports = image.DIRECTORY_ENTRY_EXPORT.symbols
    names = {item.name.decode("ascii") for item in exports if item.name}
    if len(exports) != 9 or names != EXPECTED:
        raise SystemExit(f"MSI95 ABI differs: {sorted(names)}")
    print(f"MSI95 x86 PE32 ABI OK: {len(exports)} exports, all names exact")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", type=pathlib.Path)
    audit(parser.parse_args().dll)
