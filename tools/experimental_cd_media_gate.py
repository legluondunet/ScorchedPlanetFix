#!/usr/bin/env python3
"""EXPERIMENTAL CD-media gate bypass for user's own retail Scorched Planet.

Makes an opt-in new executable COPY ONLY; never installs, modifies the
source, starts the game, or changes original game resources. This removes
one *menu/media availability flag check*, NOT real CD data dependencies.
The 3dfx version is supported by exact SHA-256 only.
"""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import sys

KNOWN = {
    "fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a": (
        "original", "7131d74a46fed18a7ff9837e431211548fdffb224d96a5c76a91fb752d9b6000"
    ),
    "0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45": (
        "FadeLow", "4dc4de1f9034e17b266da7b64a83ba897b4dca9e8cf9a8654f5988d02d1e0613"
    ),
}
GATE_VA = 0x43581B
ORIGINAL_BYTES = bytes.fromhex("33 c0")  # xor eax,eax : media test failed
TEST_BYTES = bytes.fromhex("b0 01")      # mov al,1 : return value guaranteed nonzero


def sha256(value):
    return hashlib.sha256(value).hexdigest()


def gate_offset(raw):
    if raw[:2] != b"MZ" or len(raw) < 0x100:
        raise ValueError("Missing PE DOS header")
    pe = struct.unpack_from("<I", raw, 0x3c)[0]
    if pe + 24 > len(raw) or raw[pe:pe+4] != b"PE\x00\x00":
        raise ValueError("Missing PE signature")
    n = struct.unpack_from("<H", raw, pe+6)[0]
    opt_size = struct.unpack_from("<H", raw, pe+20)[0]
    optional = pe + 24
    if optional + opt_size + 40*n > len(raw):
        raise ValueError("Truncated PE headers")
    if struct.unpack_from("<H", raw, optional)[0] != 0x10b:
        raise ValueError("Expected 32-bit PE")
    base = struct.unpack_from("<I", raw, optional+28)[0]
    if base != 0x400000:
        raise ValueError("Wrong image base")
    rva = GATE_VA - base
    for i in range(n):
        off = optional + opt_size + 40*i
        name = raw[off:off+8].split(b"\x00", 1)[0]
        vsize, srva, rawsize, rawptr = struct.unpack_from("<IIII", raw, off+8)
        if name == b".text" and srva <= rva and rva+2 <= srva+min(vsize,rawsize):
            position = rawptr + rva - srva
            if position+2 <= len(raw):
                return position
    raise ValueError("Gate not in backed .text section")


def make_test_copy(source):
    ident = sha256(source)
    if ident not in KNOWN:
        raise ValueError(f"Unknown input SHA-256 {ident}; refusing modification")
    name, expected = KNOWN[ident]
    position = gate_offset(source)
    if position != 0x34c1b or source[position:position+2] != ORIGINAL_BYTES:
        raise ValueError("Expected CD check signature missing")
    result = source[:position] + TEST_BYTES + source[position+2:]
    if sha256(result) != expected or len(result) != len(source):
        raise ValueError("Patched checksum / length mismatch")
    return name, result, position


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("exe", type=Path, help="original or FadeLow SPVOODOO.EXE")
    ap.add_argument("--output", required=True, type=Path,
                    help="new output file; refuses overwrites")
    ap.add_argument("--experimental", action="store_true",
                    help="required acknowledgment: not validated without CD")
    args = ap.parse_args()
    if not args.experimental:
        ap.error("To generate untested copy, pass --experimental")
    try:
        if args.exe.is_symlink() or not args.exe.is_file() or args.output.is_symlink():
            raise ValueError("Symlink or non-regular executable refused")
        if args.exe.resolve() == args.output.resolve():
            raise ValueError("Input and output must differ")
        source = args.exe.read_bytes()
        mode, patched, position = make_test_copy(source)
        flags = os.O_CREAT | os.O_EXCL | os.O_WRONLY
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        fd = os.open(args.output, flags, 0o600)
        try:
            with os.fdopen(fd, "wb") as out:
                out.write(patched)
                out.flush()
                os.fsync(out.fileno())
            if sha256(args.output.read_bytes()) != sha256(patched):
                raise ValueError("Output verification failed")
        except BaseException:
            args.output.unlink(missing_ok=True)
            raise
    except (OSError, ValueError, struct.error) as exc:
        ap.exit(1, f"ERROR: {exc}\n")
    print(f"Source mode: {mode}")
    print(f"Original SHA-256: {sha256(source)}")
    print(f"Output SHA-256:   {sha256(patched)}")
    print(f"Only byte positions 0x{position:x}-0x{position+1:x} altered")
    print(f"Created: {args.output}")
    print("UNTESTED with CD removed; rollback: use the unchanged source executable.")


if __name__ == "__main__":
    sys.exit(main())
