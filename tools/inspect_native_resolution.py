#!/usr/bin/env python3
"""Read-only native resolution inventory for original/FadeLow Scorched Planet 3dfx.

Only built-in Python + optional objdump. Does NOT patch the game or run Wine.
"""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess

KNOWN_SHA256 = {
    "fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a": "original",
    "0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45": "FadeLow",
}
# Real instruction boundaries, verified against the original EXE.
SIGNATURES = {
    0x434B5C: ("width 640 initialization", "c7 05 d4 46 57 00 80 02 00 00"),
    0x434B66: ("height 480 initialization", "c7 05 2c 8f 52 00 e0 01 00 00"),
    0x434C4A: ("RenderWare object creation receives width/height",
               "6a 00 a1 2c 8f 52 00 50 8b 0d d4 46 57 00 51 e8 f2 9b 05 00"),
    0x434E50: ("640x480x16 predicate", "83 7c 24 0c 10 75 1c 81 7c 24 04 80 02 00 00 75 12"),
    0x4305B5: ("predicate used by display-transition caller", "e8 96 48 00 00 85 c0"),
    0x434A90: ("load dynamically named rwdlvood device", "68 c4 8f 4b 00 e8 96 5e 06 00"),
    0x48E943: ("indirect RenderWare device vtable call", "ff 53 2c 85 c0"),
    0x435840: ("HUD viewport width branch", "81 3d d4 46 57 00 80 02 00 00"),
    0x4358BD: ("640-wide HUD viewport", "c7 00 80 02 00 00"),
    0x4358E5: ("400-high HUD viewport", "c7 00 90 01 00 00"),
}
STRINGS = {0x4B8FC4: "rwdlvood", 0x4BB604: "RWDEVICE"}
CROSSREFS = {
    "shared render width": 0x5746D4,
    "shared render height": 0x528F2C,
    "HUD viewport helper": 0x4358F0,
    "640x480x16 predicate": 0x434E50,
    "RenderWare object factory": 0x48E850,
}

def pe_sections(data):
    if len(data) < 0x100 or data[:2] != b"MZ":
        raise ValueError("missing MZ signature")
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if pe + 24 > len(data) or data[pe:pe+4] != b"PE\x00\x00":
        raise ValueError("missing PE signature")
    n = struct.unpack_from("<H", data, pe+6)[0]
    optsz = struct.unpack_from("<H", data, pe+20)[0]
    opt = pe+24
    if opt+optsz+n*40 > len(data) or struct.unpack_from("<H", data, opt)[0] != 0x10b:
        raise ValueError("unexpected PE format/section table")
    base=struct.unpack_from("<I", data, opt+28)[0]
    sec=[]
    for i in range(n):
        p=opt+optsz+i*40
        name=data[p:p+8].split(b"\0")[0].decode("ascii", "replace")
        vsize,rva,rawsz,rawptr=struct.unpack_from("<IIII", data, p+8)
        sec.append((name,base+rva,min(vsize,rawsz),rawptr))
    return base,sec

def file_offset(va, sections):
    for name,base,size,raw in sections:
        if base <= va < base + size:
            return raw+va-base
    raise ValueError(f"unmapped address {va:#x}")

def verify(data, known_only=True):
    sha=hashlib.sha256(data).hexdigest()
    if known_only and sha not in KNOWN_SHA256:
        raise ValueError("unsupported EXE SHA-256: "+sha)
    base,sec=pe_sections(data)
    if base!=0x400000:
        raise ValueError("unexpected image base")
    for va,(label,hex_bytes) in SIGNATURES.items():
        actual=bytes.fromhex(hex_bytes)
        p=file_offset(va,sec)
        if data[p:p+len(actual)] != actual:
            raise ValueError(f"{label}: instruction signature mismatch at {va:#x}")
    for va,string in STRINGS.items():
        p=file_offset(va,sec)
        if data[p:p+len(string)+1] != string.encode()+b"\0":
            raise ValueError(f"missing {string} at {va:#x}")
    return KNOWN_SHA256.get(sha, "synthetic"),sec

def refs(path):
    try:
        output=subprocess.run(["objdump","-d","-M","intel",str(path)],
                              capture_output=True,text=True,check=True).stdout
    except (OSError,subprocess.CalledProcessError):
        return None
    lines=[x for x in output.splitlines() if re.match(r"^\s*[0-9a-f]+:",x)]
    return {name:[x.strip() for x in lines if re.search(
        rf"(?<![0-9a-f])0x{va:x}(?![0-9a-f])",x,re.I)]
        for name,va in CROSSREFS.items()}

def self_test():
    fixture=bytearray(0xc0000)
    fixture[:2]=b"MZ"
    struct.pack_into("<I",fixture,0x3c,0x80)
    fixture[0x80:0x84]=b"PE\x00\x00"
    struct.pack_into("<H",fixture,0x86,2)
    struct.pack_into("<H",fixture,0x94,0xe0)
    struct.pack_into("<H",fixture,0x98,0x10b)
    struct.pack_into("<I",fixture,0x80+24+28,0x400000)
    s=0x80+24+0xe0
    fixture[s:s+8]=b".text\0\0\0"
    struct.pack_into("<IIII",fixture,s+8,0xb2000,0x1000,0xb2000,0x400)
    fixture[s+40:s+48]=b".data\0\0\0"
    struct.pack_into("<IIII",fixture,s+48,0x7000,0xb7000,0x7000,0xb5400)
    sec=pe_sections(fixture)[1]
    for va,(_,value) in SIGNATURES.items():
        pos=file_offset(va,sec)
        b=bytes.fromhex(value)
        fixture[pos:pos+len(b)]=b
    for va,text in STRINGS.items():
        pos=file_offset(va,sec)
        fixture[pos:pos+len(text)+1]=text.encode()+b"\0"
    assert verify(fixture,known_only=False)[0] == "synthetic"
    assert file_offset(0x434B5C,sec)==0x33F5C
    assert file_offset(0x434E50,sec)==0x34250
    assert file_offset(0x4B8FC4,sec)==0xB73C4
    fixture[0x33F5C+7]^=1
    try:
        verify(fixture,known_only=False)
        raise AssertionError("corrupted instruction not rejected")
    except ValueError as e:
        assert "mismatch" in str(e)
    print("OK: synthetic PE signatures, offsets, string checks and corruption refusal")

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("exe",nargs="?",type=Path)
    ap.add_argument("--self-test",action="store_true")
    args=ap.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.exe:
        ap.error("provide original or FadeLow SPVOODOO.EXE (no DLL needed)")
    raw=args.exe.read_bytes()
    mode,sec=verify(raw)
    print(f"{mode}: verified exact SHA256 {hashlib.sha256(raw).hexdigest()}")
    for va,(label,_) in SIGNATURES.items():
        print(f"  {va:#010x} -> file {file_offset(va,sec):#x}: {label}")
    result=refs(args.exe)
    if result is None:
        print("objdump unavailable; skipping xrefs")
    else:
        for label,lines in result.items():
            print(f"  {label}: {len(lines)} instruction references")
            if label=="640x480x16 predicate":
                for line in lines:
                    print(f"    {line}")
    print("NO MODIFICATIONS. Exact RWDLVOOD.DLL required to analyze 3dfx mode selection.")

if __name__=="__main__":
    try:
        main()
    except (OSError,ValueError,struct.error) as exc:
        raise SystemExit(f"ERROR: {exc}")
