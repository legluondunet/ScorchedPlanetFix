/* Copyright (C) 2026 ScorchedPlanetFix contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Optional x86 software-rasterizer edge coverage rule: screen y increases
 * downward; top/left edges inclusive, bottom/right edges exclusive.
 * Shared by clean-room MSI95 renderer and a synthetic execution smoke. */
#ifndef MSI_EDGE_TOP_LEFT_H
#define MSI_EDGE_TOP_LEFT_H
static int msi_edge_top_left(float ax,float ay,float bx,float by){
    float dy=by-ay,dx=bx-ax;
    return dy<0.f || (dy==0.f && dx>0.f);
}
/* Barycentrics correspond to opposite edges (B,C),(C,A),(A,B).
 * Winding reversal reverses each edge, preserving exactly one writer
 * along a shared edge, while leaving UV/color interpolation unchanged.
 * Narrow tolerance permits floating-point edge equality, not area fixes. */
static int msi_top_left_accept(float w0,float w1,float w2,
        float x0,float y0,float x1,float y1,float x2,float y2,
        float den){
    const float e=0.00001f;
    if(w0 < -e || w1 < -e || w2 < -e)return 0;
    if(den>0.f){
        if(w0<=e && !msi_edge_top_left(x1,y1,x2,y2))return 0;
        if(w1<=e && !msi_edge_top_left(x2,y2,x0,y0))return 0;
        if(w2<=e && !msi_edge_top_left(x0,y0,x1,y1))return 0;
    }else{
        if(w0<=e && !msi_edge_top_left(x2,y2,x1,y1))return 0;
        if(w1<=e && !msi_edge_top_left(x0,y0,x2,y2))return 0;
        if(w2<=e && !msi_edge_top_left(x1,y1,x0,y0))return 0;
    }
    return 1;
}
#endif
