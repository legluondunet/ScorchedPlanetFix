/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pure RGB565 comparison for offline replay and future in-process shadow rendering.
 * No allocation, Vulkan calls, CRT math, logging or guest memory writes.
 * Caller supplies validated draw bounds/vertices and three equal-size buffers.
 */
#ifndef MSI_GPU_COMPARE_H
#define MSI_GPU_COMPARE_H
#include "msi_gpu_draw_batch.h"
#define MSI_GPU_COMPARE_EXAMPLES 12u
typedef struct {unsigned x,y;uint16_t expected,actual;} MsiGpuCompareExample;
typedef struct {
    unsigned exact,one,edge,bad,outside,cpu_changed,gpu_changed;
    unsigned active,active_exact,cpu_only,gpu_only,both_changed;
    unsigned perspective,bilinear,affine_nearest,modulated,dithered,keyed;
    MsiGpuCompareExample examples[MSI_GPU_COMPARE_EXAMPLES];
} MsiGpuCompare;
static unsigned msi_gpu_compare_diff(unsigned a,unsigned b){return a>b?a-b:b-a;}
static unsigned msi_gpu_compare_channel(uint16_t a,uint16_t b){
    unsigned r=msi_gpu_compare_diff(a>>11,b>>11);
    unsigned g=msi_gpu_compare_diff((a>>5)&63u,(b>>5)&63u);
    unsigned bl=msi_gpu_compare_diff(a&31u,b&31u);
    return r>g?(r>bl?r:bl):(g>bl?g:bl);
}
static int msi_gpu_compare_edge(const MsiGpuVertex *v,unsigned x,unsigned y){
    for(unsigned k=0;k<3;k++){
        unsigned n=(k+1u)%3u;
        double dx=(double)v[n].xy[0]-v[k].xy[0],dy=(double)v[n].xy[1]-v[k].xy[1];
        double len2=dx*dx+dy*dy;
        double cross=dx*(y+.5-v[k].xy[1])-dy*(x+.5-v[k].xy[0]);
        /* Same 0.05 native-pixel line-distance tolerance as replay;
         * square both sides to avoid an external sqrt/fabs dependency. */
        if(len2>0.0&&cross*cross<=.05*.05*len2)return 1;
    }
    return 0;
}
static void msi_gpu_compare(MsiGpuCompare *r,const MsiGpuDrawBatch *b,
    unsigned w,unsigned h,const uint16_t *before,const uint16_t *cpu,const uint16_t *gpu){
    *r=(MsiGpuCompare){0};
    for(unsigned j=0;j<b->count;j++){
        unsigned f=b->draws[j].flags;
        r->perspective+=!!(f&1u);r->bilinear+=!!(f&8u);r->affine_nearest+=!(f&9u);
        r->modulated+=!!(f&16u);r->dithered+=!!(f&32u);r->keyed+=!!(f&64u);
    }
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        size_t pos=(size_t)y*w+x;uint16_t want=cpu[pos],got=gpu[pos];
        unsigned cc=want!=before[pos],gc=got!=before[pos];
        r->cpu_changed+=cc;r->gpu_changed+=gc;
        r->active+=cc||gc;r->cpu_only+=cc&&!gc;r->gpu_only+=gc&&!cc;r->both_changed+=cc&&gc;
        if(want==got){r->exact++;r->active_exact+=cc||gc;continue;}
        int in_bounds=0,near_edge=0;
        for(unsigned j=0;j<b->count;j++){
            const MsiGpuRect q=b->draws[j].bounds;
            if(x<q.x0||x>=q.x1||y<q.y0||y>=q.y1)continue;
            in_bounds=1;
            if(msi_gpu_compare_edge(&b->vertices[3u*j],x,y))near_edge=1;
        }
        if(!in_bounds){r->outside++;continue;}
        if(near_edge){r->edge++;continue;}
        if(msi_gpu_compare_channel(got,want)==1u){r->one++;continue;}
        if(r->bad<MSI_GPU_COMPARE_EXAMPLES)r->examples[r->bad]=(MsiGpuCompareExample){x,y,want,got};
        r->bad++;
    }
}
static int msi_gpu_compare_counts_ok(const MsiGpuCompare *r,unsigned pixels){
    return r->active==r->cpu_only+r->gpu_only+r->both_changed&&
        r->cpu_changed==r->cpu_only+r->both_changed&&r->gpu_changed==r->gpu_only+r->both_changed&&
        r->active==r->active_exact+r->one+r->edge+r->bad+r->outside&&
        r->active<=pixels&&r->exact==pixels-r->active+r->active_exact;
}
static int msi_gpu_compare_pass(const MsiGpuCompare *r,unsigned pixels){
    return msi_gpu_compare_counts_ok(r,pixels)&&!r->bad&&!r->outside&&r->cpu_changed>0&&r->gpu_changed>0;
}
#endif
