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
#endif
