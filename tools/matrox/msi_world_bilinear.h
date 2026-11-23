/* SPDX-License-Identifier: GPL-3.0-or-later
 * Optional MODERN texture-filter arithmetic. The original Mystique
 * used nearest sampling; this is not a claim of historical fidelity.
 *
 * Pure freestanding RGB565 math: callers resolve original CLUT first,
 * then pass four verified opaque texels and 8-bit fractional weights.
 * Called only for four independently confirmed opaque neighboring
 * texels in eligible perspective-varying 3D world triangles. The game
 * may still request transparency for an otherwise opaque world texture.
 * Never interpolate across a keyed texel or on UI/HUD Z-planes.
 */
#ifndef SCORCHED_MSI_WORLD_BILINEAR_H
#define SCORCHED_MSI_WORLD_BILINEAR_H
static unsigned msi_bilinear_mix_rgb565(unsigned c00,unsigned c10,
                                        unsigned c01,unsigned c11,
                                        unsigned wx,unsigned wy){
    if(wx>256u)wx=256u;
    if(wy>256u)wy=256u;
    unsigned w00=(256u-wx)*(256u-wy), w10=wx*(256u-wy);
    unsigned w01=(256u-wx)*wy, w11=wx*wy;
    unsigned r=((c00>>11)*w00+(c10>>11)*w10+
                (c01>>11)*w01+(c11>>11)*w11+32768u)>>16;
    unsigned g=(((c00>>5)&63u)*w00+((c10>>5)&63u)*w10+
                ((c01>>5)&63u)*w01+((c11>>5)&63u)*w11+32768u)>>16;
    unsigned b=((c00&31u)*w00+(c10&31u)*w10+
                (c01&31u)*w01+(c11&31u)*w11+32768u)>>16;
    return (r<<11)|(g<<5)|b;
}
#endif
