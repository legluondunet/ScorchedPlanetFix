/* SPDX-License-Identifier: GPL-3.0-or-later
 * Transactional retirement proof for recorded opaque batches.
 * Caller owns disjoint buffers, immutable snapshots, synchronous GPU
 * output, and EXCLUSIVE guest framebuffer ownership. No deferred
 * execution or guest memory protection is provided here.
 */
#ifndef MSI_GPU_DEFERRED_FINISH_H
#define MSI_GPU_DEFERRED_FINISH_H
#include "msi_gpu_cpu_replay.h"
#include "msi_gpu_bypass.h"
enum {MSI_DEFER_FINISH_BLOCKED=0,MSI_DEFER_FINISH_GPU=1,MSI_DEFER_FINISH_CPU=2};
static inline int msi_gpu_deferred_finish(MsiGpuBypass *result,const MsiGpuDrawBatch *b,
    unsigned w,unsigned h,uint16_t *live,uint8_t *underlay,const uint16_t *before,
    int gpu_ok,const uint32_t *tagged,uint16_t *cpu,uint8_t *mask){
    *result=(MsiGpuBypass){0};
    if(!b||!b->count||b->count>32u||!w||!h||w>4096u||h>4096u||
       !live||!before||!underlay||!cpu||!mask||
       live==before||live==cpu||before==cpu||underlay==mask)return MSI_DEFER_FINISH_BLOCKED;
    size_t n=(size_t)w*h;
    /* Never overwrite guest changes with an older baseline. A stale
     * framebuffer is a contract violation, not a recoverable GPU fault. */
    for(size_t j=0;j<n;j++)if(live[j]!=before[j])return MSI_DEFER_FINISH_BLOCKED;
    if(gpu_ok&&tagged&&msi_gpu_bypass_tagged_batch_commit(result,w,h,b,live,underlay,before,tagged))
        return MSI_DEFER_FINISH_GPU;
    /* Failed or rejected GPU output has made no changes to live/underlay. */
    if(!msi_gpu_cpu_replay(b,w,h,before,cpu,mask))return MSI_DEFER_FINISH_BLOCKED;
    unsigned written=0,changed=0;
    for(size_t j=0;j<n;j++){
        if(live[j]!=before[j]||mask[j]>2u||(!mask[j]&&cpu[j]!=before[j]))
            return MSI_DEFER_FINISH_BLOCKED;
        written+=mask[j]!=0;changed+=cpu[j]!=before[j];
    }
    for(size_t j=0;j<n;j++)if(mask[j]){
        live[j]=cpu[j];underlay[j]=mask[j];
    }
    result->written=written;result->changed=changed;
    return MSI_DEFER_FINISH_CPU;
}
#endif
