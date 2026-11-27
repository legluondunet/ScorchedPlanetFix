/* SPDX-License-Identifier: GPL-3.0-or-later
 * Opaque wave write coverage from two independent initial RGB565 images.
 * The second initial image is bitwise complement of the first. An opaque
 * successful fragment writes the same color on both; a discard preserves
 * the two different initial colors. No alpha or framebuffer reads admitted.
 */
#ifndef MSI_GPU_COVERAGE_H
#define MSI_GPU_COVERAGE_H
#include "msi_gpu_compare.h"
typedef struct {unsigned cpu_written,gpu_written,both,cpu_only,gpu_only,
    same_color,edge,bad,outside,invalid;} MsiGpuCoverage;
static inline void msi_gpu_coverage(MsiGpuCoverage *r,const MsiGpuDrawBatch *b,
    unsigned w,unsigned h,const uint8_t *cpu_mask,const uint16_t *before,
    const uint16_t *cpu,const uint16_t *gpu,const uint16_t *alternate){
    *r=(MsiGpuCoverage){0};
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        size_t j=(size_t)y*w+x;
        unsigned cw=cpu_mask[j]!=0,gw=gpu[j]==alternate[j];
        unsigned inverted=(unsigned)before[j]^65535u;
        if(!gw&&(gpu[j]!=before[j]||alternate[j]!=inverted)){
            r->invalid++;continue;
        }
        r->cpu_written+=cw;r->gpu_written+=gw;r->both+=cw&&gw;
        r->cpu_only+=cw&&!gw;r->gpu_only+=gw&&!cw;
        r->same_color+=cw&&cpu[j]==before[j];
        if(cw==gw)continue;
        int in=0,edge=0;
        for(unsigned k=0;k<b->count;k++){
            MsiGpuRect q=b->draws[k].bounds;
            if(x<q.x0||x>=q.x1||y<q.y0||y>=q.y1)continue;
            in=1;edge|=msi_gpu_compare_edge(&b->vertices[3u*k],x,y);
        }
        if(!in)r->outside++;
        else if(edge)r->edge++;
        else r->bad++;
    }
}
static inline int msi_gpu_coverage_pass(const MsiGpuCoverage *r){
    return !r->bad&&!r->outside&&!r->invalid&&
        r->cpu_written==r->both+r->cpu_only&&r->gpu_written==r->both+r->gpu_only&&
        r->cpu_only+r->gpu_only==r->edge;
}
/* Cross-check tagged R32 against both independent R16 replays.
 * cpu_mask records the last successful CPU write category (1/2).
 * A homogeneous batch has unambiguous provenance even with overlap.
 * Mixed categories are counted but never guessed from final color. */
typedef struct {unsigned written,untouched,color_diff,coverage_diff,invalid,provenance_diff,homogeneous,provenance_checked;} MsiGpuTaggedCheck;
static inline void msi_gpu_tagged_check(MsiGpuTaggedCheck *r,const MsiGpuDrawBatch *b,
    unsigned n,const uint8_t *cpu_mask,const uint16_t *a,const uint16_t *alt,const uint32_t *tagged){
    *r=(MsiGpuTaggedCheck){0};
    if(!b->count){r->invalid=1;return;}
    unsigned category=(b->draws[0].flags&64u)?2u:1u;
    r->homogeneous=1;
    for(unsigned k=0;k<b->count;k++)
        if(((b->draws[k].flags&64u)?2u:1u)!=category)r->homogeneous=0;
    for(unsigned j=0;j<n;j++){
        if(tagged[j]>65536u||cpu_mask[j]>2u){r->invalid++;continue;}
        unsigned written=tagged[j]!=65536u;
        r->written+=written;r->untouched+=!written;
        if(written!=(a[j]==alt[j]))r->coverage_diff++;
        else if(written&&tagged[j]!=a[j])r->color_diff++;
        if(r->homogeneous&&written&&cpu_mask[j]){
            r->provenance_checked++;
            if(cpu_mask[j]!=category)r->provenance_diff++;
        }
    }
}
static inline int msi_gpu_tagged_pass(const MsiGpuTaggedCheck *r,unsigned n){
    return r->written+r->untouched==n&&r->written&&
        !r->color_diff&&!r->coverage_diff&&!r->invalid&&!r->provenance_diff;
}
#endif
