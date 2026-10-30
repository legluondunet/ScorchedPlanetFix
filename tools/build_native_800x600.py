#!/usr/bin/env python3
"""Experimental Scorched Planet 800x600 builder. READ THE WARNINGS.

Input: exact validated original RenderWare 3dfx DLL and FadeLow game EXE.
Output: two NEW files in a NEW directory; no overwrite, no game launch.
Commercial binaries are not part of this repository.
"""
import argparse
import hashlib
import os
from pathlib import Path
import struct

GAME_SHA = "0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45"
RW_SHA = "1bd95e0dfeaaa47c53d5e2d4206027968790b928949321efd1eb2bb70f9201de"
GAME_800_SHA = "67f4b9d2bb8132bd0799ff91e1c50bc130f3df75265f98de44ef91db79e5bfad"
RW_800_SHA = "4dd3c3f0b4a009b428de059aa5f373db79c6da10b5dba922f64e246c060c3321"

# All keys are VAs of immediate operands, not instruction starts.
# Value tuples are (old value, new value, operand length).
GAME_PATCHES = {
    0x4349FA:(640,800,4),  # window center X
    0x434A0D:(480,600,4),  # window center Y
    0x434A20:(640,800,4),  # window rectangle right
    0x434A38:(480,600,4),  # window rectangle bottom
    0x434B62:(640,800,4),  # RenderWare globals: width
    0x434B6C:(480,600,4),  # RenderWare globals: height
}
RW_PATCHES = {
    0x100013F4:(7,8,1),     # grSstOpen@24 resolution mode enum
    0x10001454:(480,600,4),# RenderWare device creation
    0x10001459:(640,800,4),
    0x1000193C:(640,800,4),# 16-bit mode descriptor
    0x10001949:(480,600,4),
}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def sections(raw):
    if len(raw) < 0x100 or raw[:2] != b"MZ":
        raise ValueError("not an MZ executable")
    pe = struct.unpack_from("<I",raw,0x3C)[0]
    if pe > len(raw)-24 or raw[pe:pe+4] != b"PE\x00\x00":
        raise ValueError("not a PE")
    n = struct.unpack_from("<H",raw,pe+6)[0]
    optsize = struct.unpack_from("<H",raw,pe+20)[0]
    opt = pe+24
    if opt+optsize+n*40 > len(raw) or struct.unpack_from("<H",raw,opt)[0] != 0x10B:
        raise ValueError("not a valid PE32 section table")
    base = struct.unpack_from("<I",raw,opt+28)[0]
    out = []
    for i in range(n):
        s = opt+optsize+40*i
        name = raw[s:s+8].split(b"\0")[0]
        size,rva,rawsize,ptr = struct.unpack_from("<IIII",raw,s+8)
        out.append((name,base+rva,min(size,rawsize),ptr))
    return out

def raw_offset(pe_sections,va,n):
    for name,start,size,ptr in pe_sections:
        if name==b".text" and start <= va and va+n <= start+size:
            return ptr+va-start
    raise ValueError("address not backed by .text: "+hex(va))

def build(raw,expected_hash,patches,output_hash=None):
    if sha(raw)!=expected_hash:
        raise ValueError("unknown source SHA-256; refusing")
    sec=sections(raw)
    out=bytearray(raw)
    permitted=set()
    for va,(old,new,n) in patches.items():
        pos=raw_offset(sec,va,n)
        if raw[pos:pos+n]!=old.to_bytes(n,"little"):
            raise ValueError(f"expected byte signature missing at {va:#x}")
        out[pos:pos+n]=new.to_bytes(n,"little")
        permitted.update(range(pos,pos+n))
    changes=[i for i,(a,b) in enumerate(zip(raw,out)) if a!=b]
    if not changes or any(x not in permitted for x in changes):
        raise ValueError("unexpected changed bytes")
    if output_hash is not None and sha(out)!=output_hash:
        raise ValueError("patched SHA-256 mismatch")
    assert len(raw)==len(out)
    return bytes(out),changes

def write_new(path,data):
    flags=os.O_WRONLY|os.O_CREAT|os.O_EXCL
    if hasattr(os,"O_NOFOLLOW"):flags|=os.O_NOFOLLOW
    fd=os.open(path,flags,0o600)
    try:
        with os.fdopen(fd,"wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        if sha(path.read_bytes())!=sha(data):
            raise ValueError("output verification failed")
    except BaseException:
        path.unlink(missing_ok=True)
        raise

def selftest():
    # Synthetic PE fixture; no proprietary game/driver data.
    for base,patches in ((0x400000,GAME_PATCHES),(0x10000000,RW_PATCHES)):
        raw=bytearray(0x50000)
        raw[:2]=b"MZ"
        struct.pack_into("<I",raw,0x3C,0x80)
        raw[0x80:0x84]=b"PE\x00\x00"
        struct.pack_into("<H",raw,0x86,1)
        struct.pack_into("<H",raw,0x94,0xE0)
        struct.pack_into("<H",raw,0x98,0x10B)
        struct.pack_into("<I",raw,0x80+24+28,base)
        s=0x80+24+0xE0
        raw[s:s+8]=b".text\x00\x00\x00"
        struct.pack_into("<IIII",raw,s+8,0x49000,0x1000,0x49000,0x400)
        sect=sections(raw)
        for va,(old,new,n) in patches.items():
            off=raw_offset(sect,va,n)
            raw[off:off+n]=old.to_bytes(n,"little")
        clean=bytes(raw)
        updated,changed=build(clean,sha(clean),patches)
        assert updated!=clean and changed
        for va,(old,new,n) in patches.items():
            off=raw_offset(sect,va,n)
            assert updated[off:off+n]==new.to_bytes(n,"little")
        try:
            build(clean,"0"*64,patches)
            raise AssertionError("unknown hash not rejected")
        except ValueError as e:
            assert "unknown source" in str(e)
        raw[raw_offset(sect,next(iter(patches)),list(patches.values())[0][2])]^=1
        try:
            build(bytes(raw),sha(raw),patches)
            raise AssertionError("incorrect signature not rejected")
        except ValueError as e:
            assert "signature" in str(e)
    print("Synthetic PE validation PASS for game and RenderWare patches")

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--fadelow",type=Path)
    ap.add_argument("--renderware",type=Path)
    ap.add_argument("--output-dir",type=Path)
    ap.add_argument("--experimental",action="store_true")
    ap.add_argument("--self-test",action="store_true")
    a=ap.parse_args()
    if a.self_test:
        selftest()
        return
    if not a.experimental:
        ap.error("requires explicit --experimental (untested in game)")
    if not a.fadelow or not a.renderware or not a.output_dir:
        ap.error("requires --fadelow, --renderware, and --output-dir")
    for path in (a.fadelow,a.renderware):
        if not path.is_file() or path.is_symlink():
            ap.error(f"unsafe or missing input: {path}")
    exe,exediff=build(a.fadelow.read_bytes(),GAME_SHA,GAME_PATCHES,GAME_800_SHA)
    dll,dlldiff=build(a.renderware.read_bytes(),RW_SHA,RW_PATCHES,RW_800_SHA)
    if a.output_dir.exists() or a.output_dir.is_symlink():
        ap.error("output directory already exists; refusing overwrite")
    a.output_dir.mkdir(parents=True,exist_ok=False)
    try:
        write_new(a.output_dir/"SPVOODOO.EXE",exe)
        write_new(a.output_dir/"RWDLVOOD.DLL",dll)
    except BaseException:
        for name in ("SPVOODOO.EXE","RWDLVOOD.DLL"):
            (a.output_dir/name).unlink(missing_ok=True)
        a.output_dir.rmdir()
        raise
    print(f"GAME: SHA256 {sha(exe)}; changed {len(exediff)} bytes")
    print(f"RENDERWARE: SHA256 {sha(dll)}; changed {len(dlldiff)} bytes")
    print("Experimental only: use in a SEPARATE COPY of the game, never overwrite originals.")

if __name__=="__main__":
    main()
