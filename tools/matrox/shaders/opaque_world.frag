#version 450
// SPDX-License-Identifier: GPL-3.0-or-later
// No blending, depth or HUD here. Host must reject those draws.
// Input R32_UINT: RGB565 low 16, bit 16 center discard, bit 17 filter veto.
// Host resolves CLUT, residency and original index/color key exceptions.
// Output is packed RGB565 in an R16_UINT color attachment for readback.
layout(set=0,binding=0) uniform usampler2D sourceTex;
layout(location=0) noperspective in vec2 uvWeighted;
layout(location=1) noperspective in float invW;
layout(location=2) noperspective in vec3 modifier;
layout(location=0) out uint packedColor;
layout(push_constant) uniform Draw {
    vec2 logicalExtent;
    vec2 targetExtent;
    uint flags; // perspective=1, clamp U=2, clamp V=4, bilinear=8, modulate=16, dither=32, key metadata=64
    uint reserved;
} draw;
const uint bayer[16]=uint[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
uvec3 unpack565(uint c){return uvec3(c>>11u,(c>>5u)&63u,c&31u);}
uint pack565(uvec3 c){return (c.r<<11u)|(c.g<<5u)|c.b;}
int address(int p,int n,bool clamped){
    if(clamped)return clamp(p,0,n-1);
    return ((p%n)+n)%n;
}
uint tap(ivec2 p,ivec2 size){
    p.x=address(p.x,size.x,(draw.flags&2u)!=0u);
    p.y=address(p.y,size.y,(draw.flags&4u)!=0u);
    return texelFetch(sourceTex,p,0).r;
}
uint mix565(uvec3 a,uvec3 b,uvec3 c,uvec3 d,uvec2 w){
    uint w00=(256u-w.x)*(256u-w.y), w10=w.x*(256u-w.y);
    uint w01=(256u-w.x)*w.y,w11=w.x*w.y;
    return pack565((a*w00+b*w10+c*w01+d*w11+uvec3(32768u))>>16u);
}
uint sample565(vec2 uv){
    ivec2 size=textureSize(sourceTex,0);
    uv.x=(draw.flags&2u)!=0u?clamp(uv.x,0.0,1.0):fract(uv.x);
    uv.y=(draw.flags&4u)!=0u?clamp(uv.y,0.0,1.0):fract(uv.y);
    ivec2 center=min(ivec2(uv*vec2(size)),size-1);
    uint nearest=tap(center,size);
    bool keyed=(draw.flags&64u)!=0u;
    if(keyed&&(nearest&0x10000u)!=0u)discard;
    if((draw.flags&8u)==0u)return nearest&65535u;
    vec2 p=uv*vec2(size)-0.5;
    ivec2 i=ivec2(floor(p));
    uvec2 w=uvec2(fract(p)*256.0+0.5);
    uint a=tap(i,size),b=tap(i+ivec2(1,0),size);
    uint c=tap(i+ivec2(0,1),size),d=tap(i+ivec2(1,1),size);
    // A visible center may still veto interpolation (index exceptions).
    if(keyed&&((a|b|c|d)&0x30000u)!=0u)return nearest&65535u;
    return mix565(unpack565(a&65535u),unpack565(b&65535u),
        unpack565(c&65535u),unpack565(d&65535u),w);
}
void main(){
    // Host validates finite, positive invW for perspective draws.
    uint c=sample565(uvWeighted/invW);
    if((draw.flags&16u)!=0u){
        // Modifiers are affine 0..255, not normalized or perspective corrected.
        uvec3 m=uvec3(clamp(modifier,0.0,255.0)+0.5);
        uvec3 n=unpack565(c)*m;
        if((draw.flags&32u)!=0u){
            // Map the exact center of each destination pixel to a logical
            // pixel using integer arithmetic. Float division can round just
            // below an integer boundary on hardware (e.g. x=706 -> 314).
            // Host bounds both extents to 16384, so the product fits uint32.
            uvec2 pixel=uvec2(gl_FragCoord.xy);
            uvec2 p=((2u*pixel+1u)*uvec2(draw.logicalExtent))/(2u*uvec2(draw.targetExtent));
            uint threshold=((2u*bayer[(p.y&3u)*4u+(p.x&3u)]+1u)*255u)/32u;
            uvec3 q=n/255u;
            uvec3 r=n%255u;
            c=pack565(min(q+uvec3(greaterThan(r,uvec3(threshold))),uvec3(31,63,31)));
        }else c=pack565((n+uvec3(127u))/255u);
    }
    packedColor=c;
}
