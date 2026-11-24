#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare independent fresh-frame MSI95 dither conditions (synthetic pixels only)."""
import argparse
import json
import struct
from pathlib import Path
from check_lighting_probe import COLORS, LEVELS, model_8bit, ordered_color, undithered_color

REQUEST=(0,1,1,0,0,0,1,1)
OVERRIDE=(255,255,255,255,0,1,0,1)
RECORD=4100

def analyze(raw, software=False, rgb888=False):
    if not raw.startswith(b"M95DITH1") or len(raw) not in (8+4*RECORD,8+8*RECORD):
        raise ValueError("Incomplete or invalid dither matrix")
    results=[]
    samples=[(s,l,x,y) for row,s in enumerate(COLORS) for col,l in enumerate(LEVELS)
             for y in range(8+22*row+3,8+22*row+7)
             for x in range(16+38*col+5,16+38*col+9)]
    all_pixels=[]
    for f,offset in enumerate(range(8,len(raw),RECORD)):
        if raw[offset:offset+4]!=bytes((f,REQUEST[f],OVERRIDE[f],0)):
            raise ValueError("Unexpected dither condition header")
        pix=struct.unpack_from("<2048H",raw,offset+4)
        all_pixels.append(pix)
        counts={}
        for d in (0,1):
            counts[f"native565_d{d}"]=sum(v==(ordered_color(s,int(l+.5),int(l+.5),int(l+.5),x,y) if d else undithered_color(s,int(l+.5),int(l+.5),int(l+.5))) for v,(s,l,x,y) in zip(pix,samples))
            for py in range(2 if d else 1):
                for px in range(2 if d else 1):
                    counts[f"rgb888_d{d}_phase{px}{py}"]=sum(v==model_8bit(s,int(l+.5),x+px,y+py,d) for v,(s,l,x,y) in zip(pix,samples))
        varying=sum(len(set(pix[i:i+16]))>1 for i in range(0,2048,16))
        expected=(f"rgb888_d{REQUEST[f]}_phase00" if rgb888 else f"native565_d{REQUEST[f]}")
        if software and counts[expected]!=2048:
            raise ValueError(f"Software reference mismatch frame {f}: {counts}")
        results.append({"frame":f,"requested":REQUEST[f],"override":None if OVERRIDE[f]==255 else OVERRIDE[f],
                        "varying_cells":varying,"model_matches":counts})
    if software and len(results)!=4:
        raise ValueError("Unexpected direct setter in current software DLL")
    return {"frames":results,"exact_frame_pairs":[[a,b] for a in range(len(all_pixels)) for b in range(a+1,len(all_pixels)) if all_pixels[a]==all_pixels[b]]}

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("file",type=Path)
    p.add_argument("--software",action="store_true")
    p.add_argument("--rgb888",action="store_true",help="expect experimental software RGB888, unchanged request polarity")
    a=p.parse_args()
    print(json.dumps(analyze(a.file.read_bytes(),a.software,a.rgb888),indent=2))
