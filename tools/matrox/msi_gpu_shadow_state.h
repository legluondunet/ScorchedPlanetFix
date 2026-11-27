/* SPDX-License-Identifier: GPL-3.0-or-later
 * Experimental packed color/ownership CPU reference for a future GPU target.
 * This mirrors successful writes of the existing world rasterizer, not
 * physical Matrox hardware. Skipped/keyed/zero-opacity fragments preserve
 * both fields. Caller has already evaluated texture, lighting and keying.
 */
#ifndef MSI_GPU_SHADOW_STATE_H
#define MSI_GPU_SHADOW_STATE_H
#include <stdint.h>
#define MSI_GPU_SHADOW_BLEND 1u
#define MSI_GPU_SHADOW_UNDERLAY 2u
#define MSI_GPU_SHADOW_ONCE 4u
static uint32_t msi_gpu_shadow_write(uint32_t old,unsigned color,unsigned opacity,
    unsigned textured,unsigned zplane,unsigned transparent,unsigned modes,unsigned discarded){
    if(discarded||!opacity)return old;
    if(opacity>100u)opacity=100u;
    unsigned prior=(old>>16)&3u;
    unsigned candidate=!zplane&&!textured&&color==0x0841u;
    unsigned applied=opacity;
    if(candidate&&opacity==100u){
        if((modes&MSI_GPU_SHADOW_ONCE)&&prior==3u)return old;
        if((modes&MSI_GPU_SHADOW_BLEND)||
           ((modes&MSI_GPU_SHADOW_UNDERLAY)&&prior)||
           ((modes&MSI_GPU_SHADOW_ONCE)&&(prior==1u||prior==2u)))applied=50u;
    }
    if(applied<100u){
        unsigned inv=100u-applied,dst=old&65535u;
        unsigned r=(((color>>11)*applied+(dst>>11)*inv)+50u)/100u;
        unsigned g=((((color>>5)&63u)*applied+((dst>>5)&63u)*inv)+50u)/100u;
        unsigned b=(((color&31u)*applied+(dst&31u)*inv)+50u)/100u;
        color=(r<<11)|(g<<5)|b;
    }
    unsigned tag=(!zplane&&textured)?(transparent?2u:1u):0u;
    if((modes&MSI_GPU_SHADOW_ONCE)&&candidate&&opacity==100u&&applied==50u)tag=3u;
    return color|(tag<<16);
}
#endif
