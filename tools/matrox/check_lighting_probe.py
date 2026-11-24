#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate the synthetic probe against the current software model only."""
import struct
import sys
from pathlib import Path
from analyze_lighting_capture import ordered_color, undithered_color
from analyze_f12_capture import read_rgb565_bmp

COLORS=(0xffff,0x8410,0x4208,0x0841,0xf800,0x07e0,0x001f,0x7bef)
LEVELS=(0,.25,.5,1,2,3,4,7,8,15,16,31,63,127,254,255)

def validate(path):
    w,h,pix=read_rgb565_bmp(path)
    assert (w,h)==(640,400), (w,h)
    checked=0
    for d in range(2):
        for row,source in enumerate(COLORS):
            for col,light in enumerate(LEVELS):
                m=int(light+.5)
                # Stay well inside cell, away from edges and shared diagonal.
                for y in range(8+22*(row+8*d)+3,8+22*(row+8*d)+7):
                    for x in range(16+38*col+5,16+38*col+9):
                        expected=(ordered_color(source,m,m,m,x,y) if d else
                                  undithered_color(source,m,m,m))
                        actual=pix[y*w+x]
                        assert actual==expected,(d,row,col,x,y,hex(actual),hex(expected))
                        checked+=1
    # Check the monotonicity/endpoints of each Gouraud ramp without making
    # undocumented assumptions about original hardware raster edge rules.
    for d in range(2):
        y=366+16*d
        ramp=[pix[y*w+x] for x in range(20,620)]
        assert ramp[0]<0x1082 and ramp[-1]>0xef5d,(d,ramp[0],ramp[-1])
        if not d:
            assert all(a<=b for a,b in zip(ramp,ramp[1:])), "nonmonotonic undithered ramp"
    print(f"MSI95_LIGHTING_PROBE_PASS {checked} exact interior pixels plus Gouraud ramps")

if __name__=="__main__":
    validate(Path(sys.argv[1]))
