/* SPDX-License-Identifier: GPL-3.0-or-later
 * Transactional single opaque triangle publication. Two independent GPU
 * readbacks on complementary backgrounds expose actual fragment writes.
 * This checks structural validity, not numerical equivalence to the CPU.
 * Caller owns all nonoverlapping buffers and synchronizes the guest.
 */
#ifndef MSI_GPU_BYPASS_H
#define MSI_GPU_BYPASS_H
#include "msi_gpu_draw_batch.h"
typedef struct {unsigned written,changed;} MsiGpuBypass;
static inline int msi_gpu_bypass_commit(MsiGpuBypass *r,unsigned w,unsigned h,
    const MsiGpuDrawBatch *b,uint16_t *live,uint8_t *underlay,
    const uint16_t *before,const uint16_t *gpu,const uint16_t *alt){
    *r=(MsiGpuBypass){0};
    if(!b||b->count!=1u||!w||!h||w>4096u||h>4096u||
       !live||!before||!gpu||!alt||live==before||live==gpu||live==alt||
       before==gpu||before==alt||gpu==alt)return 0;
    const MsiGpuDraw *d=&b->draws[0];MsiGpuRect q=d->bounds;
    if((d->flags&~127u)||!(d->flags&1u)||!(d->flags&8u)||
       q.x0>=q.x1||q.y0>=q.y1||q.x1>w||q.y1>h)return 0;
    unsigned written=0,changed=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        size_t j=(size_t)y*w+x;
        if(live[j]!=before[j])return 0;
        if(gpu[j]==alt[j]){
            if(x<q.x0||x>=q.x1||y<q.y0||y>=q.y1)return 0;
            written++;changed+=gpu[j]!=before[j];
        }else if(gpu[j]!=before[j]||alt[j]!=(uint16_t)(before[j]^65535u))return 0;
    }
    if(!written)return 0;
    /* Commit only after the whole transaction has been validated. Even
     * same-color writes replace old shadow provenance. Discards do not. */
    for(unsigned y=q.y0;y<q.y1;y++)for(unsigned x=q.x0;x<q.x1;x++){
        size_t j=(size_t)y*w+x;
        if(gpu[j]!=alt[j])continue;
        live[j]=gpu[j];if(underlay)underlay[j]=(d->flags&64u)?2u:1u;
    }
    r->written=written;r->changed=changed;return 1;
}
/* R32_UINT sentinel=65536; all valid RGB565 colors, including zero
 * and 65535, are distinct from no-write. No initial image upload. */
static inline int msi_gpu_bypass_tagged_commit(MsiGpuBypass *r,unsigned w,unsigned h,
    const MsiGpuDrawBatch *b,uint16_t *live,uint8_t *underlay,
    const uint16_t *before,const uint32_t *gpu){
    *r=(MsiGpuBypass){0};
    if(!b||b->count!=1u||!w||!h||w>4096u||h>4096u||
       !live||!before||!gpu||live==before||(const void*)live==(const void*)gpu||
       (const void*)before==(const void*)gpu)return 0;
    const MsiGpuDraw *d=&b->draws[0];MsiGpuRect q=d->bounds;
    if((d->flags&~127u)||!(d->flags&1u)||!(d->flags&8u)||
       q.x0>=q.x1||q.y0>=q.y1||q.x1>w||q.y1>h)return 0;
    unsigned written=0,changed=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        size_t j=(size_t)y*w+x;
        if(live[j]!=before[j])return 0;
        if(gpu[j]==65536u)continue;
        if(gpu[j]>65535u||x<q.x0||x>=q.x1||y<q.y0||y>=q.y1)return 0;
        written++;changed+=gpu[j]!=before[j];
    }
    if(!written)return 0;
    for(unsigned y=q.y0;y<q.y1;y++)for(unsigned x=q.x0;x<q.x1;x++){
        size_t j=(size_t)y*w+x;
        if(gpu[j]==65536u)continue;
        live[j]=(uint16_t)gpu[j];if(underlay)underlay[j]=(d->flags&64u)?2u:1u;
    }
    r->written=written;r->changed=changed;return 1;
}
/* Packed native ROI publication for the synchronous pilot. Preserve the
 * entire live/baseline comparison, reject invalid/outside-bound tags even
 * in the guard ring, and mutate only after both checks succeed. */
static inline int msi_gpu_bypass_tagged_roi_commit(MsiGpuBypass *r,
    const MsiGpuDrawBatch *b,uint16_t *live,uint8_t *underlay,
    const uint16_t *before,const uint32_t *gpu,size_t count,MsiGpuRect roi){
    *r=(MsiGpuBypass){0};
    MsiGpuRect expected;
    if(!b||b->count!=1u||!live||!before||!gpu||live==before||
       (const void*)live==(const void*)gpu||(const void*)before==(const void*)gpu||
       !msi_gpu_native_tagged_region(b,&expected)||
       roi.x0!=expected.x0||roi.y0!=expected.y0||roi.x1!=expected.x1||roi.y1!=expected.y1)
        return 0;
    const MsiGpuDraw *d=&b->draws[0];MsiGpuRect q=d->bounds;
    if((d->flags&~127u)||(d->flags&9u)!=9u)return 0;
    unsigned rw=roi.x1-roi.x0,rh=roi.y1-roi.y0;
    if(count!=(size_t)rw*rh)return 0;
    for(size_t j=0;j<640u*400u;j++)if(live[j]!=before[j])return 0;
    unsigned written=0,changed=0;
    for(unsigned y=roi.y0;y<roi.y1;y++)for(unsigned x=roi.x0;x<roi.x1;x++){
        uint32_t tag=gpu[(size_t)(y-roi.y0)*rw+x-roi.x0];
        if(tag==65536u)continue;
        if(tag>65535u||x<q.x0||x>=q.x1||y<q.y0||y>=q.y1)return 0;
        written++;changed+=tag!=before[(size_t)y*640u+x];
    }
    if(!written)return 0;
    for(unsigned y=q.y0;y<q.y1;y++)for(unsigned x=q.x0;x<q.x1;x++){
        uint32_t tag=gpu[(size_t)(y-roi.y0)*rw+x-roi.x0];
        if(tag==65536u)continue;
        size_t j=(size_t)y*640u+x;
        live[j]=(uint16_t)tag;if(underlay)underlay[j]=(d->flags&64u)?2u:1u;
    }
    r->written=written;r->changed=changed;return 1;
}
/* Future bounded batch path. All draws must have the same shadow
 * provenance category. Mixed categories require a last-writer tag;
 * reject them rather than guessing ownership from final color.
 * Validate against the union of individual bounds, not just its AABB.
 * Caller must guarantee order and immutable textures and flush before
 * guest reads/writes; this helper does not itself defer guest draws. */
static inline int msi_gpu_bypass_tagged_batch_commit(MsiGpuBypass *r,unsigned w,unsigned h,
    const MsiGpuDrawBatch *b,uint16_t *live,uint8_t *underlay,
    const uint16_t *before,const uint32_t *gpu){
    *r=(MsiGpuBypass){0};
    if(!b||!b->count||b->count>32u||!w||!h||w>4096u||h>4096u||
       !live||!before||!gpu||live==before||(const void*)live==(const void*)gpu||
       (const void*)before==(const void*)gpu)return 0;
    const unsigned key=b->draws[0].flags&64u;
    for(unsigned k=0;k<b->count;k++){
        const MsiGpuDraw *d=&b->draws[k];MsiGpuRect q=d->bounds;
        if((d->flags&~127u)||(d->flags&64u)!=key||(d->flags&9u)!=9u||
           q.x0>=q.x1||q.y0>=q.y1||q.x1>w||q.y1>h)return 0;
    }
    unsigned written=0,changed=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        size_t j=(size_t)y*w+x;
        if(live[j]!=before[j])return 0;
        if(gpu[j]==65536u)continue;
        if(gpu[j]>65535u)return 0;
        int in=0;
        for(unsigned k=0;k<b->count&&!in;k++){
            MsiGpuRect q=b->draws[k].bounds;
            in=x>=q.x0&&x<q.x1&&y>=q.y0&&y<q.y1;
        }
        if(!in)return 0;
        written++;changed+=gpu[j]!=before[j];
    }
    if(!written)return 0;
    for(size_t j=0;j<(size_t)w*h;j++){
        if(gpu[j]==65536u)continue;
        live[j]=(uint16_t)gpu[j];if(underlay)underlay[j]=key?2u:1u;
    }
    r->written=written;r->changed=changed;return 1;
}
#endif
