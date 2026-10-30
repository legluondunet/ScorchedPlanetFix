#!/usr/bin/env python3
"""Read-only verification of retail Scorched Planet's palette-fade timing.

Never runs or alters the game. Requires only the Python standard library.
Use --self-test for CI without the commercial binary.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import sys

EXPECTED_SHA256 = "fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a"

# Entries taken from actual objdump instruction boundaries, not a blind
# pattern scan: 12 matching 'jne -22' loops call the same fade function.
FADE_LOOP_BRANCHES = (
    0x4193D0, 0x4195F5, 0x419DC6, 0x419EBC,
    0x41FED9, 0x426DB1, 0x427D38, 0x430602,
    0x43067F, 0x456BB9, 0x457B37, 0x4586A7,
)

# For this build, image_base=0x400000, .text RVA=0x1000 and file_offset=0x400.
# Never assume these values for another build.
SIGS = (
    (0x434D1C, bytes.fromhex("ff1598f35700"), "QueryPerformanceFrequency IAT call"),
    (0x434D5F, bytes.fromhex("dc0df0484b00"), "scale QPC by constant double"),
    (0x434D65, bytes.fromhex("d91d58bc5700"), "save adjusted QPC frequency"),
    (0x432B2F, bytes.fromhex("ff15d4f35700"), "QueryPerformanceCounter IAT call"),
    (0x432B8A, bytes.fromhex("d83558bc5700"), "divide elapsed QPC ticks"),
    (0x4495C6, bytes.fromhex("81bc240404000017b7d138"), "minimum delta gate"),
    (0x4496F4, bytes.fromhex("813dc8994c006f12833a"), "remaining-fade threshold"),
)

def f32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]

def get_section_offsets(data):
    if data[:2] != b"MZ":
        raise ValueError("not a DOS/PE MZ header")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\x00\x00":
        raise ValueError("not a PE signature")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    imagebase = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    sections = []
    for i in range(count):
        p = pe + 24 + optsize + 40 * i
        name = data[p:p + 8].split(b"\0")[0].decode("ascii", "replace")
        virtual_size, rva, raw_size, raw_off = struct.unpack_from("<IIII", data, p + 8)
        sections.append((name, rva, virtual_size, raw_off, raw_size))
    return imagebase, sections

def file_offset(addr, imagebase, sections):
    rva = addr - imagebase
    for name, start, virtual_size, raw_off, raw_size in sections:
        if start <= rva < start + min(virtual_size, raw_size):
            return raw_off + rva - start
    raise ValueError(f"{addr:#x} not backed by raw file data")

def check_original(data):
    actual = hashlib.sha256(data).hexdigest()
    if actual != EXPECTED_SHA256:
        raise ValueError(f"unsupported EXE sha256 {actual}; expected {EXPECTED_SHA256}")
    base, sections = get_section_offsets(data)
    if base != 0x400000:
        raise ValueError(f"unexpected image base: {base:#x}")
    for addr, sig, label in SIGS:
        p = file_offset(addr, base, sections)
        if data[p:p + len(sig)] != sig:
            raise ValueError(f"{label} signature mismatch at {addr:#x}")
    for addr in FADE_LOOP_BRANCHES:
        p = file_offset(addr, base, sections)
        if data[p:p + 2] != b"\x75\xea":
            raise ValueError(f"backward palette fade branch mismatch at {addr:#x}")
    c = file_offset(0x4B48F0, base, sections)
    factor = struct.unpack_from("<d", data, c)[0]
    if factor != 1 / 12:
        raise ValueError(f"unexpected QPC normalization: {factor}")
    low_off = file_offset(0x4495CD, base, sections)
    end_off = file_offset(0x4496FA, base, sections)
    threshold = struct.unpack_from("<f", data, low_off)[0]
    end_threshold = struct.unpack_from("<f", data, end_off)[0]
    if not (0 < threshold < end_threshold):
        raise ValueError("unexpected fade time thresholds")
    return factor, threshold, end_threshold, low_off

def model_polling(microseconds, calls, freq=10_000_000, gate=None):
    # Deliberately simplified: model the QPC delta and fade counter,
    # not the actual game, OS scheduler, or 256-entry palette writes.
    if gate is None:
        gate = f32(struct.unpack("<f", bytes.fromhex("17b7d138"))[0])
    scaled_frequency = f32(freq / 12.0)
    tickstep = round(microseconds * freq / 1_000_000)
    remaining = f32(3.0)
    accepted = 0
    for _ in range(calls):
        delta = f32(tickstep / scaled_frequency)
        if delta > gate:
            remaining = f32(max(0.0, remaining - min(delta, remaining)))
            accepted += 1
        if remaining <= f32(0.001):
            break
    return remaining, accepted, tickstep, delta

def self_test():
    factor, threshold = 1 / 12, f32(0.0001)
    assert factor == 0.08333333333333333
    assert 8e-6 * 12 < threshold < 9e-6 * 12
    for us in (1, 5, 8):
        rem, accepted, _, _ = model_polling(us, 300_000, gate=threshold)
        assert rem == 3.0 and accepted == 0, (us, rem, accepted)
    for us in (9, 20):
        rem, accepted, _, _ = model_polling(us, 300_000, gate=threshold)
        assert rem <= 0.001 and accepted > 0, (us, rem, accepted)
    assert len(FADE_LOOP_BRANCHES) == 12
    print("Self-test: 12 branch locations and toy timing model OK.")

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("exe", nargs="?", help="retail SPVOODOO.EXE")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.exe:
        ap.error("provide the original EXE or --self-test")
    data = Path(args.exe).read_bytes()
    factor, gate, end, gate_off = check_original(data)
    print("Verified exact original SPVOODOO.EXE SHA-256 and byte signatures.")
    print(f"QPC scale: {factor:g} of the real counter frequency")
    print(f"Fade gate: {gate:.12g} game units = {gate / 12 * 1e6:.6g} microseconds real time")
    print(f"End threshold: {end:.12g} game units")
    print(f"Fade loop back-edges: {len(FADE_LOOP_BRANCHES)}")
    print(f"Gate immediate raw file offset: 0x{gate_off:06x}; read-only verification only")
    print("Demo: 300000 deterministic polls, 10MHz QPC, remaining=3 game units")
    for us in (1, 5, 8, 9, 20):
        rem, accepted, ticks, delta = model_polling(us, 300_000, gate=gate)
        print(f"  {us:2} us/poll: delta={delta:.8g}, accepted={accepted}, remaining={rem:.8g}")
    print("CAUTION: this model does not establish the actual Go-freeze cause.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
