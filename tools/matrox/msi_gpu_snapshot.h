/* SPDX-License-Identifier: GPL-3.0-or-later
 * Bounded immutable indexed texture snapshots for an in-flight draw batch.
 * Caller owns buffers, validates guest residency, and retires the batch
 * only after its last GPU consumer completes. Offset/address is NOT identity.
 * A miss stores both original bytes and decoded R32_UINT texels; hash is
 * merely an acceleration, never proof of identity. No allocation or eviction.
 */
#ifndef MSI_GPU_SNAPSHOT_H
#define MSI_GPU_SNAPSHOT_H
#include "msi_gpu_texel.h"
#define MSI_GPU_SNAPSHOT_MAX 128u
typedef struct {
    MsiGpuTexture state;
    uint32_t hash;
    size_t raw_offset,raw_count,decoded_offset,decoded_count;
} MsiGpuSnapshot;
typedef struct {
    MsiGpuSnapshot entries[MSI_GPU_SNAPSHOT_MAX];
    unsigned count;
    uint8_t *raw;
    uint32_t *decoded;
    size_t raw_used,raw_capacity,decoded_used,decoded_capacity;
} MsiGpuSnapshots;
enum { MSI_GPU_SNAPSHOT_INVALID=0, MSI_GPU_SNAPSHOT_HIT=1,
       MSI_GPU_SNAPSHOT_NEW=2, MSI_GPU_SNAPSHOT_FULL=3 };
static void msi_gpu_snapshot_reset(MsiGpuSnapshots *c){
    c->count=0;c->raw_used=0;c->decoded_used=0;
}
static uint32_t msi_gpu_snapshot_hash(const uint8_t *p,size_t n,uint32_t hash){
    for(size_t i=0;i<n;i++)hash=(hash^p[i])*16777619u;
    return hash;
}
static int msi_gpu_snapshot_equal(const uint8_t *a,const uint8_t *b,size_t n){
    for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static int msi_gpu_snapshot_state_equal(const MsiGpuTexture *a,const MsiGpuTexture *b){
    return a->width==b->width&&a->height==b->height&&a->planes==b->planes&&
        a->transparent==b->transparent&&a->key==b->key&&a->mask==b->mask&&
        a->index_key==b->index_key&&a->blue_key==b->blue_key&&a->icon01_match==b->icon01_match;
}
/* src/lut must not alias arenas; guest memory must remain readable and
 * stable for this synchronous call. No output or arena change on FULL/INVALID.
 * An ID is valid only until reset. Never reset an in-flight batch. */
static int msi_gpu_snapshot_indexed8(MsiGpuSnapshots *c,const MsiGpuTexture *s,
    const uint8_t *src,size_t src_size,const uint8_t *lut,size_t lut_size,unsigned *id){
    if(!c||!s||!src||!lut||!id||!c->raw||!c->decoded||s->planes!=8u||
       !s->width||!s->height||s->width>2048u||s->height>2048u||
       c->count>MSI_GPU_SNAPSHOT_MAX||c->raw_used>c->raw_capacity||
       c->decoded_used>c->decoded_capacity)return MSI_GPU_SNAPSHOT_INVALID;
    size_t n=(size_t)s->width*s->height;
    if(n>1024u*1024u||src_size<n||lut_size<512u)return MSI_GPU_SNAPSHOT_INVALID;
    uint32_t hash=msi_gpu_snapshot_hash(lut,512u,msi_gpu_snapshot_hash(src,n,2166136261u));
    for(unsigned i=0;i<c->count;i++){
        const MsiGpuSnapshot *e=&c->entries[i];
        if(e->hash==hash&&msi_gpu_snapshot_state_equal(&e->state,s)&&
           msi_gpu_snapshot_equal(c->raw+e->raw_offset,src,n)&&
           msi_gpu_snapshot_equal(c->raw+e->raw_offset+n,lut,512u)){
            *id=i;return MSI_GPU_SNAPSHOT_HIT;
        }
    }
    if(c->count==MSI_GPU_SNAPSHOT_MAX||n+512u>c->raw_capacity-c->raw_used||
       n>c->decoded_capacity-c->decoded_used)return MSI_GPU_SNAPSHOT_FULL;
    size_t ro=c->raw_used,doff=c->decoded_used;
    for(size_t i=0;i<n;i++)c->raw[ro+i]=src[i];
    for(size_t i=0;i<512u;i++)c->raw[ro+n+i]=lut[i];
    if(!msi_gpu_decode_world(s,c->raw+ro,n,c->raw+ro+n,512u,c->decoded+doff,n))
        return MSI_GPU_SNAPSHOT_INVALID; /* prevalidated; unreachable */
    MsiGpuSnapshot *e=&c->entries[c->count];
    e->state=*s;e->hash=hash;e->raw_offset=ro;e->raw_count=n+512u;
    e->decoded_offset=doff;e->decoded_count=n;
    *id=c->count++;c->raw_used+=n+512u;c->decoded_used+=n;
    return MSI_GPU_SNAPSHOT_NEW;
}
#endif
