/* SPDX-License-Identifier: GPL-3.0-or-later
 * Conservative first-pilot admission only. No ownership proof. Limit
 * perspective conditioning so every accepted interior pixel (including
 * the CPU rasterizer's -1e-5 edge tolerance) stays within the CPU
 * replay's finite coordinate domain. Rejected draws MUST remain immediate.
 */
#ifndef MSI_GPU_DEFER_ADMIT_H
#define MSI_GPU_DEFER_ADMIT_H
#include "msi_gpu_draw_batch.h"
/* Half-open ranges, no overflowing endpoint arithmetic. Callers already
 * validated the source span; this is an extra conservative alias veto. */
static inline int msi_gpu_defer_overlap(uintptr_t p,size_t bytes,uintptr_t base,size_t size){
    if(!bytes||!size)return 0;
    return p>=base?p-base<size:base-p<bytes;
}
static inline int msi_gpu_defer_draw_admissible(const MsiGpuVertex v[3],MsiGpuRect r,unsigned flags){
    if(!v||(flags&~127u)||(flags&9u)!=9u||
       r.x0>=r.x1||r.y0>=r.y1||r.x1>640u||r.y1>400u)return 0;
    float lo=v[0].iw,hi=v[0].iw;
    for(unsigned i=0;i<3;i++){
        if(!msi_gpu_vertex_valid(&v[i],flags)||
           !(v[i].iw>=0.0001f&&v[i].iw<=1024.f))return 0;
        for(unsigned j=0;j<2;j++)
            if(!(v[i].uv[j]>=-128.f&&v[i].uv[j]<=128.f))return 0;
        if(v[i].iw<lo)lo=v[i].iw;
        if(v[i].iw>hi)hi=v[i].iw;
    }
    if(hi>64.f*lo)return 0;
    /* Exclude ill-conditioned nearly degenerate geometry. Coordinates
     * are already bounded by the recorder, so products cannot overflow. */
    float bx=v[1].xy[0]-v[0].xy[0],by=v[1].xy[1]-v[0].xy[1];
    float cx=v[2].xy[0]-v[0].xy[0],cy=v[2].xy[1]-v[0].xy[1];
    float den=bx*cy-by*cx;
    return den>=0.00390625f||den<=-0.00390625f;
}
static inline int msi_gpu_defer_batch_admissible(const MsiGpuDrawBatch *b){
    if(!b||!b->count||b->count>32u||!b->snapshots.decoded||
       !b->snapshots.count||b->snapshots.count>MSI_GPU_SNAPSHOT_MAX||
       b->snapshots.decoded_used>2097152u)return 0;
    unsigned key=b->draws[0].flags&64u;
    for(unsigned k=0;k<b->count;k++){
        const MsiGpuDraw *d=&b->draws[k];
        if((d->flags&64u)!=key||d->texture_id>=b->snapshots.count||
           !msi_gpu_defer_draw_admissible(&b->vertices[3u*k],d->bounds,d->flags))return 0;
        const MsiGpuSnapshot *t=&b->snapshots.entries[d->texture_id];
        unsigned tw=t->state.width,th=t->state.height;
        if(!tw||!th||tw>2048u||th>2048u||tw*th>1048576u||
           t->decoded_count!=(size_t)tw*th||
           t->decoded_offset>b->snapshots.decoded_used||
           t->decoded_count>b->snapshots.decoded_used-t->decoded_offset)return 0;
    }
    for(size_t i=0;i<b->snapshots.decoded_used;i++)
        if(b->snapshots.decoded[i]&~0x3ffffu)return 0;
    return 1;
}
#endif
