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
/* Native tagged region: union declared bounds with conservative vertex
 * bounds, then add a one-pixel guard ring. Shared by dispatch and commit
 * so the packed buffer can never silently omit required geometry. */
static inline int msi_gpu_native_tagged_region(const MsiGpuDrawBatch *batch,MsiGpuRect *out){
    if(!batch||!out||!batch->count||batch->count>MSI_GPU_WAVE_MAX)return 0;
    MsiGpuRect r={640u,400u,0,0};
    for(unsigned i=0;i<batch->count;i++){
        MsiGpuRect q=batch->draws[i].bounds;
        if(q.x0>=q.x1||q.y0>=q.y1||q.x1>640u||q.y1>400u)return 0;
        /* Never trust the caller's bookkeeping rectangle to clip GPU
         * geometry. Union with conservative clipped vertex bounds, even
         * if a malformed record underreports its true triangle extent. */
        for(unsigned j=0;j<3u;j++){
            const MsiGpuVertex *v=&batch->vertices[3u*i+j];
            if(!msi_gpu_vertex_valid(v,batch->draws[i].flags))return 0;
            float x=v->xy[0],y=v->xy[1];
            unsigned x0=x<=0.f?0u:x>=640.f?640u:(unsigned)x;
            unsigned y0=y<=0.f?0u:y>=400.f?400u:(unsigned)y;
            unsigned x1=x>=640.f?640u:x<=0.f?0u:(unsigned)x+1u;
            unsigned y1=y>=400.f?400u:y<=0.f?0u:(unsigned)y+1u;
            /* Exact integer endpoints do not need an additional pixel;
             * the separate guard ring below covers transform rounding. */
            if(x>0.f&&x<640.f&&x==(float)x0)x1=x0;
            if(y>0.f&&y<400.f&&y==(float)y0)y1=y0;
            if(x0<q.x0)q.x0=x0;if(y0<q.y0)q.y0=y0;
            if(x1>q.x1)q.x1=x1;if(y1>q.y1)q.y1=y1;
        }
        if(q.x0<r.x0)r.x0=q.x0;if(q.y0<r.y0)r.y0=q.y0;
        if(q.x1>r.x1)r.x1=q.x1;if(q.y1>r.y1)r.y1=q.y1;
    }
    if(r.x0)r.x0--;if(r.y0)r.y0--;
    if(r.x1<640u)r.x1++;if(r.y1<400u)r.y1++;
    *out=r;return 1;
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
