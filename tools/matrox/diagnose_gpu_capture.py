#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Local M95GPU01 CPU diagnostic; no network, no third-party dependencies.

Compare legacy float32 incremental scanline weights to direct evaluation.
Neither model is a GPU emulator. Report independent CPU-reference errors,
last writer, and the input coordinates at selected mismatch pixels.
"""
import argparse
import math
import struct
from pathlib import Path

W, H = 640, 400
PIXELS = W * H
F32 = struct.Struct("<f")
BAYER = (0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5)


def f(x):
    return F32.unpack(F32.pack(x))[0]


def load(path):
    if path.stat().st_size > 10 * 1024 * 1024:
        raise ValueError("capture too large")
    b = path.read_bytes()
    if len(b) < 32 or b[:8] != b"M95GPU01":
        raise ValueError("invalid capture magic")
    w, h, n, nt, words, frame = struct.unpack_from("<6I", b, 8)
    if (w, h) != (W, H) or not (8 <= n <= 32 and 1 <= nt <= n and 1 <= words <= 2097152):
        raise ValueError("invalid header")
    if len(b) != 32 + 12 * nt + 120 * n + 4 * words + 4 * PIXELS:
        raise ValueError("invalid capture size")
    offset = 0
    records = []
    for i in range(nt):
        tw, th, base = struct.unpack_from("<3I", b, 32 + 12 * i)
        if not (1 <= tw <= 2048 and 1 <= th <= 2048 and tw * th <= 1048576 and base == offset and offset + tw * th <= words):
            raise ValueError("invalid texture")
        records.append((tw, th, base))
        offset += tw * th
    if offset != words:
        raise ValueError("invalid texture size")
    pos = 32 + 12 * nt
    draws = [struct.unpack_from("<6I", b, pos + 24 * i) for i in range(n)]
    pos += 24 * n
    vertices = [tuple(struct.unpack_from("<8f", b, pos + 96 * i + 32 * j) for j in range(3)) for i in range(n)]
    pos += 96 * n
    for (tid, flags, l, t, r, bot), vv in zip(draws, vertices):
        if not (tid < nt and not flags & ~127 and l < r <= W and t < bot <= H):
            raise ValueError("invalid draw")
        for v in vv:
            if not all(math.isfinite(x) and abs(x) <= 10000 for x in v[:4]):
                raise ValueError("invalid coordinates")
            if flags & 1 and not 1e-8 < v[4] < 1e8:
                raise ValueError("invalid inverse W")
            if flags & 16 and not all(math.isfinite(x) and abs(x) <= 100000 for x in v[5:]):
                raise ValueError("invalid modifier")
    tex = struct.unpack_from(f"<{words}I", b, pos)
    if any(v & ~0x3ffff for v in tex):
        raise ValueError("invalid texel flags")
    pos += 4 * words
    before = struct.unpack_from(f"<{PIXELS}H", b, pos)
    after = struct.unpack_from(f"<{PIXELS}H", b, pos + 2 * PIXELS)
    return frame, records, draws, vertices, tex, before, after


def rgb(c):
    return c >> 11 & 31, c >> 5 & 63, c & 31


def pack(c):
    return c[0] << 11 | c[1] << 5 | c[2]


def clamp_mod(x):
    return int(f(min(255., max(0., x)) + .5))


def cross(ax, ay, bx, by):
    return f(f(ax * by) - f(ay * bx))


def interp(w0, w1, w2, a, b, c):
    return f(f(f(w0 * a) + f(w1 * b)) + f(w2 * c))


def replay(capture, direct=False, watch=()):
    _, records, draws, vertices, tex, before, _ = capture
    out = list(before)
    writer = [-1] * PIXELS
    trace = {}
    watched = set(watch)
    for j, ((tid, flags, left, top, right, bottom), (a, b, c)) in enumerate(zip(draws, vertices)):
        tw, th, base = records[tid]
        x0, y0 = a[:2]
        bx, by, cx, cy = f(b[0]-x0), f(b[1]-y0), f(c[0]-x0), f(c[1]-y0)
        den = cross(bx, by, cx, cy)
        if abs(den) < .0001:
            continue
        inv = f(1. / den)
        step1, step2 = f(cy * inv), f(-by * inv)
        perspective = flags & 1
        clamp_u, clamp_v = flags & 2, flags & 4
        bilinear, modulate, dither, keyed = flags & 8, flags & 16, flags & 32, flags & 64
        constant_mod = a[5:] == b[5:] == c[5:]
        fm = tuple(clamp_mod(v) for v in a[5:]) if modulate and constant_mod else None
        if perspective:
            weighted_u = tuple(f(v[2]*v[4]) for v in (a,b,c))
            weighted_v = tuple(f(v[3]*v[4]) for v in (a,b,c))
        def address(i, size, clamp):
            return min(size-1,max(0,i)) if clamp else i % size
        for y in range(top, bottom):
            py = f(y + .5 - y0)
            px = f(left + .5 - x0)
            w1, w2 = f(cross(px,py,cx,cy)*inv), f(cross(bx,by,px,py)*inv)
            for x in range(left, right):
                if direct:
                    # Same float32 arithmetic, but no repeated additions.
                    px = f(x + .5 - x0)
                    w1, w2 = f(cross(px,py,cx,cy)*inv), f(cross(bx,by,px,py)*inv)
                q1, q2 = w1, w2
                w1, w2 = f(w1+step1), f(w2+step2)
                q0 = f(f(1.-q1)-q2)
                if min(q0,q1,q2) < -0.00001:
                    continue
                u = interp(q0,q1,q2,a[2],b[2],c[2])
                v = interp(q0,q1,q2,a[3],b[3],c[3])
                if perspective:
                    iw = interp(q0,q1,q2,a[4],b[4],c[4])
                    if 1e-8 < iw < 1e8:
                        u = f(interp(q0,q1,q2,*weighted_u)/iw)
                        v = f(interp(q0,q1,q2,*weighted_v)/iw)
                u = min(1.,max(0.,u)) if clamp_u else f(u-math.floor(u))
                v = min(1.,max(0.,v)) if clamp_v else f(v-math.floor(v))
                tx,ty = min(tw-1,int(f(u*tw))),min(th-1,int(f(v*th)))
                center = tex[base+ty*tw+tx]
                pos = y*W+x
                if keyed and center & 0x10000:
                    continue
                color = center & 65535
                method = "nearest"
                if bilinear:
                    sx,sy = f(f(u*tw)-.5),f(f(v*th)-.5)
                    ix,iy = math.floor(sx),math.floor(sy)
                    wx,wy = int(f(f(f(sx-ix)*256)+.5)),int(f(f(f(sy-iy)*256)+.5))
                    taps = tuple(tex[base+address(iy+dy,th,clamp_v)*tw+address(ix+dx,tw,clamp_u)] for dy,dx in ((0,0),(0,1),(1,0),(1,1)))
                    if not keyed or not any(k & 0x30000 for k in taps):
                        weights = ((256-wx)*(256-wy),wx*(256-wy),(256-wx)*wy,wx*wy)
                        channels = tuple(rgb(k) for k in taps)
                        color = pack(tuple((sum(t[ch]*weight for t,weight in zip(channels,weights))+32768)>>16 for ch in range(3)))
                        method = "bilinear"
                prelight = color
                m = (255,255,255)
                if modulate:
                    m = fm if fm is not None else tuple(clamp_mod(interp(q0,q1,q2,a[k],b[k],c[k])) for k in (5,6,7))
                    n = tuple(k*v for k,v in zip(rgb(color),m))
                    if dither:
                        threshold = (2*BAYER[(y&3)*4+(x&3)]+1)*255//32
                        color = pack(tuple(min(cap,k//255+int(k%255>threshold)) for cap,k in zip((31,63,31),n)))
                    else:
                        color = pack(tuple((k+127)//255 for k in n))
                out[pos],writer[pos] = color,j
                if pos in watched:
                    trace[pos] = (j, flags, u,v,tx,ty,method,prelight,m,color)
    return out,writer,trace


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("capture",type=Path)
    p.add_argument("--require-exact",action="store_true",help="CI synthetic fixtures only")
    args = p.parse_args()
    capture = load(args.capture)
    frame,records,draws,vertices,tex,before,after = capture
    print(f"M95GPU_CPU_DIAG frame={frame} draws={len(draws)} textures={len(records)}",flush=True)
    for i,(tid,flags,l,t,r,b) in enumerate(draws):
        print(f"DRAW {i} tex={tid} flags={flags} bounds={l},{t},{r},{b} vertices={vertices[i]}")
    watch = tuple(y*W+x for x,y in ((103,0),(108,0),(118,0),(103,1),(108,1),(118,1),(88,29),(98,29),(88,30),(98,30),(88,31),(98,31)))
    scan,sw,st = replay(capture,watch=watch)
    print("SCANLINE completed",flush=True)
    direct,dw,dt = replay(capture,direct=True,watch=watch)
    for label,buf in (("SCANLINE",scan),("DIRECT",direct)):
        bad = sum(a!=b for a,b in zip(buf,after))
        one = sum(a!=b and max(abs(x-y) for x,y in zip(rgb(a),rgb(b)))==1 for a,b in zip(buf,after))
        changed = sum(a!=b for a,b in zip(buf,before))
        print(f"CPU_{label} exact={PIXELS-bad} one_lsb={one} larger={bad-one} changed={changed}")
    print(f"CPU_MODE_DIFF pixels={sum(a!=b for a,b in zip(scan,direct))} scan_only_exact={sum(a==c and b!=c for a,b,c in zip(scan,direct,after))} direct_only_exact={sum(b==c and a!=c for a,b,c in zip(scan,direct,after))}")
    for pos in watch:
        print(f"WATCH {pos%W},{pos//W} captured={after[pos]:04x} scan={scan[pos]:04x} direct={direct[pos]:04x} scan_trace={st.get(pos)} direct_trace={dt.get(pos)}")
    # Include bounded examples for CPU-vs-capture disagreements.
    count = 0
    for pos,(a,b) in enumerate(zip(scan,after)):
        if a != b:
            print(f"CPU_CAPTURE_DIFF {pos%W},{pos//W} captured={b:04x} scan={a:04x} direct={direct[pos]:04x} writers={sw[pos]},{dw[pos]}")
            count += 1
            if count == 16:
                break
    if args.require_exact and scan != list(after):
        raise SystemExit("synthetic CPU reference mismatch")
    print("M95GPU_CPU_DIAG_COMPLETE")


if __name__ == "__main__":
    main()
