#!/usr/bin/env python3
"""Static Windows PE32 / Glide 2.11 export audit for pinned GameFrame.

This is not a game compatibility test. It never runs or patches a game EXE.
"""
import re
import sys
from pathlib import Path
import pefile

NEEDED_211 = {
    "_grGlideInit@0",
    "_grGlideShutdown@0",
    "_grSstOpen@24",
    "_grSstQueryHardware@4",
    "_grSstQueryBoards@4",
    "_grSstSelect@4",
    "_grSstPassthruMode@4",
    "_grBufferSwap@4",
    "_grLfbGetReadPtr@4",
    "_grLfbGetWritePtr@4",
    "_grLfbWriteMode@4",
    "_grLfbOrigin@4",
    "_grLfbBegin@0",
    "_grLfbEnd@0",
    "_grTexSource@16",
    "_grTexDownloadMipMap@16",
    "_grFogMode@4",
    "_guFbReadRegion@24",
    "_guFbWriteRegion@24",
}

def audit(dll: Path, definition: Path) -> None:
    expected = {
        m.group(1)
        for line in definition.read_text(encoding="utf-8").splitlines()
        if (m := re.match(r"\s*(_[A-Za-z0-9_]+@\d+)\s*=", line))
    }
    if len(expected) != 156:
        raise RuntimeError(f"Expected 156 upstream decorated Glide exports; found {len(expected)}")
    pe = pefile.PE(str(dll))
    if pe.FILE_HEADER.Machine != 0x014C or not pe.is_dll():
        raise RuntimeError("GameFrame is not a Windows x86 PE32 DLL")
    pe.parse_data_directories(
        directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]]
    )
    exported = {entry.name.decode("ascii") for entry in pe.DIRECTORY_ENTRY_EXPORT.symbols if entry.name}
    absent = sorted(expected - exported)
    required_missing = sorted(NEEDED_211 - exported)
    if absent or required_missing:
        raise RuntimeError(f"Missing exports: {absent}; critical historical imports: {required_missing}")
    print(f"GameFrame Windows x86 PE DLL: PASS; exports={len(exported)}, critical historical symbols={len(NEEDED_211)}")
    print("Not equivalent to verifying all game-specific RWDLVOOD.DLL imports or actual rendering.")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: audit_exports.py <glide2x.dll> <glide2x.def>")
    audit(Path(sys.argv[1]), Path(sys.argv[2]))
