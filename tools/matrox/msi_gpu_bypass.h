/* SPDX-License-Identifier: GPL-3.0-or-later
 * Transactional single opaque triangle publication. Two independent GPU
 * readbacks on complementary backgrounds expose actual fragment writes.
 * This checks structural validity, not numerical equivalence to the CPU.
 * Caller owns all nonoverlapping buffers and synchronizes the guest.
 */
#ifndef MSI_GPU_BYPASS_H
#define MSI_GPU_BYPASS_H
#include "msi_gpu_compare.h"
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
/* Compare a packed multi-draw region with the independent full-frame
 * tagged reference. Outside the region the implicit tag is no-write.
 * Reject malformed dimensions/capacity before reading any output. */
static inline unsigned msi_gpu_packed_native_diff(const MsiGpuDrawBatch *b,
    const uint32_t *full,const uint32_t *packed,size_t count,MsiGpuRect roi){
    MsiGpuRect expected;
    if(!full||!packed||!msi_gpu_native_tagged_region(b,&expected)||
       roi.x0!=expected.x0||roi.y0!=expected.y0||roi.x1!=expected.x1||roi.y1!=expected.y1)
        return UINT32_MAX;
    unsigned rw=roi.x1-roi.x0,rh=roi.y1-roi.y0;
    if(count!=(size_t)rw*rh)return UINT32_MAX;
    unsigned diff=0;
    for(unsigned y=0;y<400u;y++)for(unsigned x=0;x<640u;x++){
        uint32_t a=full[(size_t)y*640u+x];
        uint32_t z=x>=roi.x0&&x<roi.x1&&y>=roi.y0&&y<roi.y1?
            packed[(size_t)(y-roi.y0)*rw+x-roi.x0]:65536u;
        diff+=a!=z||a>65536u||z>65536u;
    }
    return diff;
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
    if(!msi_gpu_snapshot_equal((const uint8_t*)live,(const uint8_t*)before,640u*400u*2u))return 0;
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
/* Qualification oracle, not the live publication path. Compare actual
 * fragment writes (including same-color writes) against immutable CPU
 * replay, with the same RGB565/edge tolerances as the wave diagnostics.
 * All other color or coverage discrepancies reject this pilot attempt. */
typedef struct {
    unsigned cpu_written,gpu_written,both,exact,one,edge_color,
        cpu_only,gpu_only,edge_coverage,bad,outside,invalid;
} MsiGpuPilotOracle;
static inline int msi_gpu_pilot_oracle(MsiGpuPilotOracle *r,const MsiGpuDrawBatch *b,
    const uint16_t *cpu,const uint8_t *mask,const uint32_t *gpu,size_t count,MsiGpuRect roi){
    *r=(MsiGpuPilotOracle){0};
    MsiGpuRect expected;
    if(!b||b->count!=1u||!cpu||!mask||!gpu||
       !msi_gpu_native_tagged_region(b,&expected)||
       roi.x0!=expected.x0||roi.y0!=expected.y0||roi.x1!=expected.x1||roi.y1!=expected.y1)
        return 0;
    const MsiGpuDraw *d=&b->draws[0];MsiGpuRect q=d->bounds;
    const unsigned category=(d->flags&64u)?2u:1u;
    unsigned rw=roi.x1-roi.x0,rh=roi.y1-roi.y0;
    if((d->flags&9u)!=9u||(d->flags&~127u)||count!=(size_t)rw*rh)return 0;
    for(unsigned y=0;y<400u;y++)for(unsigned x=0;x<640u;x++){
        size_t j=(size_t)y*640u+x;
        uint32_t tag=x>=roi.x0&&x<roi.x1&&y>=roi.y0&&y<roi.y1?
            gpu[(size_t)(y-roi.y0)*rw+x-roi.x0]:65536u;
        if(tag>65536u||(mask[j]&&mask[j]!=category)){r->invalid++;continue;}
        unsigned cw=mask[j]!=0,gw=tag!=65536u;
        if(!cw&&!gw)continue;
        r->cpu_written+=cw;r->gpu_written+=gw;
        r->both+=cw&&gw;r->cpu_only+=cw&&!gw;r->gpu_only+=gw&&!cw;
        if(x<q.x0||x>=q.x1||y<q.y0||y>=q.y1){r->outside++;continue;}
        if(cw&&gw&&tag==cpu[j]){r->exact++;continue;}
        int edge=msi_gpu_compare_edge(b->vertices,x,y);
        if(cw!=gw){if(edge)r->edge_coverage++;else r->bad++;continue;}
        if(edge)r->edge_color++;
        else if(msi_gpu_compare_channel((uint16_t)tag,cpu[j])==1u)r->one++;
        else r->bad++;
    }
    return r->cpu_written&&r->gpu_written&&!r->bad&&!r->outside&&!r->invalid&&
        r->both==r->exact+r->one+r->edge_color&&
        r->cpu_only+r->gpu_only==r->edge_coverage;
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
/* Private qualification of packed homogeneous batches. Check the entire
 * native baseline and every packed tag, including holes between bounds.
 * Never mutate until all validation succeeds. Not a guest scheduling API. */
static inline int msi_gpu_packed_batch_commit(MsiGpuBypass *r,
    const MsiGpuDrawBatch *b,uint16_t *live,uint8_t *underlay,
    const uint16_t *before,const uint32_t *gpu,size_t count,MsiGpuRect roi){
    *r=(MsiGpuBypass){0};
    MsiGpuRect expected;
    if(!b||!b->count||b->count>32u||!live||!before||!gpu||live==before||
       (const void*)live==(const void*)gpu||(const void*)before==(const void*)gpu||
       !msi_gpu_native_tagged_region(b,&expected)||roi.x0!=expected.x0||roi.y0!=expected.y0||
       roi.x1!=expected.x1||roi.y1!=expected.y1)return 0;
    unsigned key=b->draws[0].flags&64u,rw=roi.x1-roi.x0,rh=roi.y1-roi.y0;
    if(count!=(size_t)rw*rh)return 0;
    for(unsigned k=0;k<b->count;k++){
        const MsiGpuDraw *d=&b->draws[k];MsiGpuRect q=d->bounds;
        if((d->flags&~127u)||(d->flags&64u)!=key||(d->flags&9u)!=9u||
           q.x0>=q.x1||q.y0>=q.y1||q.x1>640u||q.y1>400u)return 0;
    }
    if(!msi_gpu_snapshot_equal((const uint8_t*)live,(const uint8_t*)before,640u*400u*2u))return 0;
    /* Sort rectangles once, then merge active X spans per scanline.
     * The union is identical to per-pixel rectangle membership; gaps
     * (including the conservative guard ring) still require no-write. */
    MsiGpuRect sorted[32];
    for(unsigned k=0;k<b->count;k++){
        MsiGpuRect q=b->draws[k].bounds;unsigned j=k;
        while(j&&sorted[j-1u].x0>q.x0){sorted[j]=sorted[j-1u];j--;}
        sorted[j]=q;
    }
    unsigned written=0,changed=0;
    for(unsigned y=roi.y0;y<roi.y1;y++){
        unsigned starts[32],ends[32],spans=0;
        for(unsigned k=0;k<b->count;k++){
            MsiGpuRect q=sorted[k];
            if(y<q.y0||y>=q.y1)continue;
            if(spans&&q.x0<=ends[spans-1u]){
                if(q.x1>ends[spans-1u])ends[spans-1u]=q.x1;
            }else{starts[spans]=q.x0;ends[spans]=q.x1;spans++;}
        }
        const uint32_t *row=gpu+(size_t)(y-roi.y0)*rw;
        const uint16_t *old=before+(size_t)y*640u;
        unsigned x=roi.x0;
        for(unsigned k=0;k<spans;k++){
            for(;x<starts[k];x++)if(row[x-roi.x0]!=65536u)return 0;
            for(;x<ends[k];x++){
                uint32_t tag=row[x-roi.x0];
                if(tag==65536u)continue;
                if(tag>65535u)return 0;
                written++;changed+=tag!=old[x];
            }
        }
        for(;x<roi.x1;x++)if(row[x-roi.x0]!=65536u)return 0;
    }
    if(!written)return 0;
    for(unsigned y=roi.y0;y<roi.y1;y++)for(unsigned x=roi.x0;x<roi.x1;x++){
        uint32_t tag=gpu[(size_t)(y-roi.y0)*rw+x-roi.x0];
        if(tag==65536u)continue;
        size_t j=(size_t)y*640u+x;
        live[j]=(uint16_t)tag;if(underlay)underlay[j]=key?2u:1u;
    }
    r->written=written;r->changed=changed;return 1;
}
#endif
