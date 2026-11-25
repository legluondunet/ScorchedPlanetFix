/* SPDX-License-Identifier: GPL-3.0-or-later
 * World-texture snapshot decoder for the experimental Vulkan shader.
 * Pure host arithmetic: no MSI ABI, allocation, residency or GPU ownership.
 * Caller must first verify complete source/cache residency and copy at draw
 * time, including independent palette updates. Not for sparse Z-plane atlases.
 */
#ifndef MSI_GPU_TEXEL_H
#define MSI_GPU_TEXEL_H
#include <stdint.h>
#include <stddef.h>
#define MSI_GPU_DISCARD 0x10000u
#define MSI_GPU_FILTER_VETO 0x20000u
typedef struct {
    unsigned width,height,planes;
    unsigned transparent,key,mask;
    unsigned index_key,blue_key;
    /* icon01 must be enabled AND current cache offset must match its verified
       atlas. The existing index-1 exception is unconditional in world path. */
    unsigned icon01_match;
} MsiGpuTexture;
static uint32_t msi_gpu_world_texel(unsigned color,unsigned index,const MsiGpuTexture *s){
    if(!s->transparent)return color;
    unsigned indexed=s->planes==8u;
    unsigned rgb_match=((color^s->key)&s->mask)==0u;
    unsigned index_match=indexed&&s->index_key&&s->key<=255u&&s->mask!=0u&&
        (((index^s->key)&(s->mask&255u))==0u);
    unsigned blue_match=s->blue_key&&color==0x00ffu;
    unsigned exception=indexed&&s->width==128u&&s->height==128u&&
        color==0u&&s->key==0u&&s->mask==0xffffu&&
        (index==1u||(index==241u&&s->icon01_match));
    unsigned reject=(rgb_match&&!exception)||index_match||blue_match;
    unsigned veto=rgb_match||index_match||blue_match;
    return color|(reject?MSI_GPU_DISCARD:0u)|(veto?MSI_GPU_FILTER_VETO:0u);
}
static unsigned msi_gpu_le16(const uint8_t *p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
static int msi_gpu_decode_world(const MsiGpuTexture *s,const uint8_t *src,size_t src_size,
    const uint8_t *lut,size_t lut_size,uint32_t *out,size_t out_count){
    if(!s||!src||!out||!s->width||!s->height||s->width>2048u||s->height>2048u||
       (s->planes!=4u&&s->planes!=8u&&s->planes!=16u))return 0;
    size_t count=(size_t)s->width*s->height;
    size_t bytes=(count*s->planes+7u)/8u;
    size_t palette_bytes=s->planes==4u?32u:s->planes==8u?512u:0u;
    if(bytes>1024u*1024u||src_size<bytes||out_count<count||
       (palette_bytes&&(!lut||lut_size<palette_bytes)))return 0;
    for(size_t i=0;i<count;i++){
        unsigned index=0,color;
        if(s->planes==16u)color=msi_gpu_le16(src+2u*i);
        else{
            index=s->planes==8u?src[i]:((i&1u)?src[i/2u]>>4:src[i/2u]&15u);
            color=msi_gpu_le16(lut+2u*index);
        }
        out[i]=msi_gpu_world_texel(color,index,s);
    }
    return 1;
}
#endif
