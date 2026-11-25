/* SPDX-License-Identifier: GPL-3.0-or-later
 * Bounded ordered world draw recorder. Opaque/keyed indexed8 only.
 * No guest pointers survive admission. No GPU calls, no allocation.
 * Caller must flush/retire before resetting and at every guest barrier.
 */
#ifndef MSI_GPU_DRAW_BATCH_H
#define MSI_GPU_DRAW_BATCH_H
#include "msi_gpu_snapshot.h"
#include "msi_gpu_batch.h"
typedef struct {float xy[2],uv[2],iw,m[3];} MsiGpuVertex;
typedef struct {uint32_t texture_id,flags;MsiGpuRect bounds;} MsiGpuDraw;
typedef struct {
    MsiGpuSnapshots snapshots;
    MsiGpuWave wave;
    MsiGpuDraw draws[MSI_GPU_WAVE_MAX];
    MsiGpuVertex vertices[MSI_GPU_WAVE_MAX*3u];
    unsigned count,hits,misses;
} MsiGpuDrawBatch;
enum {MSI_GPU_DRAW_INVALID=0,MSI_GPU_DRAW_ACCEPT=1,MSI_GPU_DRAW_FLUSH=2};
static void msi_gpu_draw_batch_reset(MsiGpuDrawBatch *b){
    msi_gpu_snapshot_reset(&b->snapshots);msi_gpu_wave_reset(&b->wave);
    b->count=b->hits=b->misses=0;
}
static int msi_gpu_vertex_valid(const MsiGpuVertex *v,unsigned flags){
    for(unsigned i=0;i<2;i++)
        if(!(v->xy[i]>=-10000.f&&v->xy[i]<=10000.f&&v->uv[i]>=-10000.f&&v->uv[i]<=10000.f))return 0;
    if((flags&1u)&&!(v->iw>1e-8f&&v->iw<1e8f))return 0;
    if(flags&16u)for(unsigned i=0;i<3;i++)
        if(!(v->m[i]>=-100000.f&&v->m[i]<=100000.f))return 0;
    return 1;
}
/* FLUSH leaves previously accepted draws and snapshots untouched; retry
 * after retire/reset. INVALID must fall back in order, never skip a draw.
 * Source/palette must be validated by caller and stable for this call. */
static int msi_gpu_draw_batch_push(MsiGpuDrawBatch *b,const MsiGpuVertex v[3],
    MsiGpuRect bounds,unsigned flags,const MsiGpuTexture *s,
    const uint8_t *source,size_t source_size,const uint8_t *palette,size_t palette_size){
    if(!b||!v||!s||s->planes!=8u||(flags&~127u)||
       (!!(flags&64u)!=!!s->transparent)||bounds.x0>=bounds.x1||bounds.y0>=bounds.y1||
       bounds.x1>16384u||bounds.y1>16384u||b->count!=b->wave.count||b->count>MSI_GPU_WAVE_MAX)return MSI_GPU_DRAW_INVALID;
    for(unsigned i=0;i<3;i++)if(!msi_gpu_vertex_valid(&v[i],flags))return MSI_GPU_DRAW_INVALID;
    if(b->count==MSI_GPU_WAVE_MAX)return MSI_GPU_DRAW_FLUSH;
    unsigned id;
    int rc=msi_gpu_snapshot_indexed8(&b->snapshots,s,source,source_size,palette,palette_size,&id);
    if(rc==MSI_GPU_SNAPSHOT_FULL)return MSI_GPU_DRAW_FLUSH;
    if(rc!=MSI_GPU_SNAPSHOT_HIT&&rc!=MSI_GPU_SNAPSHOT_NEW)return MSI_GPU_DRAW_INVALID;
    /* Opaque-only, non-empty validated bounds, capacity prechecked. */
    if(msi_gpu_wave_push(&b->wave,bounds,0)!=MSI_GPU_WAVE_ACCEPT)return MSI_GPU_DRAW_INVALID;
    unsigned i=b->count++;
    b->draws[i].texture_id=id;b->draws[i].flags=flags;b->draws[i].bounds=bounds;
    for(unsigned j=0;j<3;j++){
        b->vertices[3u*i+j]=v[j];
        if(!(flags&1u))b->vertices[3u*i+j].iw=1.f;
        if(!(flags&16u))for(unsigned k=0;k<3;k++)b->vertices[3u*i+j].m[k]=255.f;
    }
    if(rc==MSI_GPU_SNAPSHOT_HIT)b->hits++;else b->misses++;
    return MSI_GPU_DRAW_ACCEPT;
}
#endif
