#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate a software probe or rank experimental models against a reference BMP."""
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


def samples():
    for d in range(2):
        for row, source in enumerate(COLORS):
            for col, light in enumerate(LEVELS):
                for y in range(8+22*(row+8*d)+3,8+22*(row+8*d)+7):
                    for x in range(16+38*col+5,16+38*col+9):
                        yield d,row,col,source,light,x,y

def model_8bit(source,m,x,y,d):
    # Independent arithmetic hypothesis: expand by shifts, multiply /256,
    # then quantize using a phase-dependent 2x2 pattern. Not hardware proof.
    rank=((0,3),(2,1))[y&1][x&1] if d else 0
    r=min(31,((((source>>11)&31)*8*m)//256+2*rank)//8)
    g=min(63,((((source>>5)&63)*4*m)//256+rank)//4)
    b=min(31,((source&31)*8*m//256+2*rank)//8)
    return r<<11|g<<5|b

def compare_pixels(pix):
    data=[(d,row,col,s,l,x,y,pix[y*640+x]) for d,row,col,s,l,x,y in samples()]
    # Reject blank/unrendered or grossly incorrect layouts instead of
    # presenting a best-fitting model for meaningless data.
    for d,row,col,s,l,x,y,actual in data:
        if col==0 and actual!=0:
            raise ValueError("zero-light cell is not black; layout or model unsupported")
        if col==15 and row==0 and actual<0xef5d:
            raise ValueError("white full-light cell missing or too dark")
    results=[]
    for rounding in ("nearest","truncate"):
        for model,period in (("native565",4),("rgb888_div256",2)):
            for py in range(period):
                for px in range(period):
                    matches=[0,0]
                    for d,row,col,s,l,x,y,actual in data:
                        m=int(l+.5) if rounding=="nearest" else int(l)
                        if model=="native565":
                            expected=ordered_color(s,m,m,m,x+px,y+py) if d else undithered_color(s,m,m,m)
                        else:
                            expected=model_8bit(s,m,x+px,y+py,d)
                        matches[d]+=int(actual==expected)
                    results.append({"model":model,"coefficient_rounding":rounding,
                                    "phase":[px,py],"matches":sum(matches),
                                    "no_dither_matches":matches[0],
                                    "dither_matches":matches[1]})
    results.sort(key=lambda a:-a["matches"])
    return {"sample_pixels":len(data),"ranked_models":results,
            "interpretation":"Fit to constant-cell interior pixels only. "
            "No physical-hardware, driver or interpolation proof; all models may be wrong."}

def self_test():
    pix=[0]*(640*400)
    for d,row,col,s,l,x,y in samples():
        m=int(l+.5)
        pix[y*640+x]=ordered_color(s,m,m,m,x,y) if d else undithered_color(s,m,m,m)
    result=compare_pixels(pix)
    assert result["ranked_models"][0]["matches"]==result["sample_pixels"]
    assert result["ranked_models"][0]["model"]=="native565"
    assert result["ranked_models"][0]["coefficient_rounding"]=="nearest"
    for d,row,col,s,l,x,y in samples():
        pix[y*640+x]=model_8bit(s,int(l),x+1,y,d)
    result=compare_pixels(pix)
    assert result["ranked_models"][0]["matches"]==result["sample_pixels"]
    assert result["ranked_models"][0]["model"]=="rgb888_div256"
    assert result["ranked_models"][0]["phase"]==[1,0]
    try:
        compare_pixels([0]*(640*400))
    except ValueError:
        pass
    else:
        raise AssertionError("blank image accepted")
    print("MSI95_LIGHTING_COMPARISON_SELFTEST_PASS")

if __name__=="__main__":
    import argparse
    import json
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bmp",type=Path,nargs="?")
    parser.add_argument("--compare",action="store_true")
    parser.add_argument("--self-test",action="store_true")
    args=parser.parse_args()
    if args.self_test:
        self_test()
    elif args.bmp is None:
        parser.error("BMP required")
    elif args.compare:
        w,h,pix=read_rgb565_bmp(args.bmp)
        if (w,h)!=(640,400):
            parser.error("Expected 640x400 native RGB565 probe BMP")
        print(json.dumps(compare_pixels(pix),indent=2))
    else:
        validate(args.bmp)
