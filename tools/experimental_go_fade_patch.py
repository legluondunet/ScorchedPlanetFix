#!/usr/bin/env python3
"""Experimental copy-only Scorched Planet Go palette-fade timing edit.

Accepts only the exact user-verified 1996 SPVOODOO.EXE SHA-256.
Does not run the game, overwrite source or output, or claim a working fix.
"""
import argparse
import hashlib
import os
from pathlib import Path
import struct

EXPECTED_SHA256 = "fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a"
PATCH_VA = 0x4495CD
EXPECTED_OLD = bytes.fromhex("17 b7 d1 38")
MODES = {"low": 0.00001, "zero": 0.0}


def pe_file_offset(data, va):
    if data[:2] != b"MZ":
        raise ValueError("Missing DOS MZ signature")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\x00\x00":
        raise ValueError("Missing PE signature")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    if magic != 0x10B:
        raise ValueError(f"Unexpected PE optional header: {magic:#x}")
    imagebase = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    if imagebase != 0x400000:
        raise ValueError(f"Unexpected image base: {imagebase:#x}")
    rva = va - imagebase
    for i in range(count):
        start = pe + 24 + opt_size + 40 * i
        name = data[start:start + 8].split(b"\x00", 1)[0]
        virtual_size, sec_rva, raw_size, raw_offset = struct.unpack_from("<IIII", data, start + 8)
        if name != b".text":
            continue
        if sec_rva <= rva and rva + len(EXPECTED_OLD) <= sec_rva + min(raw_size, virtual_size):
            return raw_offset + rva - sec_rva
    raise ValueError("Target VA is not in the original PE .text section")


def prepare(data, mode):
    actual = hashlib.sha256(data).hexdigest()
    if actual != EXPECTED_SHA256:
        raise ValueError(f"Refusing unsupported executable SHA-256: {actual}")
    offset = pe_file_offset(data, PATCH_VA)
    if data[offset:offset + 4] != EXPECTED_OLD:
        raise ValueError(f"Gate signature mismatch at file offset {offset:#x}")
    changed = struct.pack("<f", MODES[mode])
    patched = data[:offset] + changed + data[offset + 4:]
    diff = [i for i, (a, b) in enumerate(zip(data, patched)) if a != b]
    assert len(patched) == len(data)
    assert diff and all(offset <= i < offset + 4 for i in diff)
    return patched, offset, diff


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Unmodified retail SPVOODOO.EXE")
    parser.add_argument("--output", type=Path, required=True, help="New file (must not exist)")
    parser.add_argument("--mode", choices=sorted(MODES), default="low")
    parser.add_argument("--i-accept-the-risk", action="store_true",
                        help="Required opt-in: this is NOT tested inside the game")
    args = parser.parse_args()
    if not args.i_accept_the_risk:
        parser.error("An explicit --i-accept-the-risk is required.")
    if args.input.resolve() == args.output.resolve():
        parser.error("Input and output may not be identical")
    if not args.input.is_file():
        parser.error("Input executable not found")
    try:
        raw = args.input.read_bytes()
        updated, offset, diff = prepare(raw, args.mode)
        flags = os.O_CREAT | os.O_EXCL | os.O_WRONLY
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        fd = os.open(args.output, flags, 0o600)
        try:
            with os.fdopen(fd, "wb") as output:
                output.write(updated)
        except BaseException:
            args.output.unlink(missing_ok=True)
            raise
    except (OSError, ValueError) as exc:
        parser.exit(1, f"ERROR: {exc}\n")
    print(f"Original SHA-256: {hashlib.sha256(raw).hexdigest()}")
    print(f"Test copy SHA-256: {hashlib.sha256(updated).hexdigest()}")
    print(f"Experimental mode: {args.mode}; gate={MODES[args.mode]:.8g} game units")
    print(f"Modified VA {PATCH_VA:#x}; raw offset {offset:#x}; bytes {diff}")
    print(f"New copy saved: {args.output}")
    print("UNTESTED: do not overwrite the installed original; the test copy can be deleted.")


if __name__ == "__main__":
    main()
