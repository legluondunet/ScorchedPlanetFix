/* Copyright (C) 2026 ScorchedPlanetFix contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Optional, deterministic RGB565 ordered quantization for 8-bit Gouraud RGB.
 * This is an experimental software approximation, NOT a measured Mystique
 * hardware dither matrix. The caller gates it on the original MSI Dither
 * request, non-textured world geometry, and an explicit opt-in. */
#ifndef MSI_DITHER_GOURAUD_H
#define MSI_DITHER_GOURAUD_H
static unsigned msi_gouraud_ordered_channel(unsigned c,unsigned max,unsigned threshold){
    unsigned v=c*max;
    unsigned q=v/255u,rem=v%255u;
    if(rem>threshold && q<max)q++;
    return q;
}
static unsigned msi_gouraud_ordered_565(unsigned r,unsigned g,unsigned b,
                                        unsigned x,unsigned y){
    static const unsigned char bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    unsigned matrix=bayer[((y&3u)<<2u)|(x&3u)];
    unsigned threshold=((2u*matrix+1u)*255u)/32u;
    unsigned rr=msi_gouraud_ordered_channel(r,31u,threshold);
    unsigned gg=msi_gouraud_ordered_channel(g,63u,threshold);
    unsigned bb=msi_gouraud_ordered_channel(b,31u,threshold);
    return (rr<<11)|(gg<<5)|bb;
}
#endif
