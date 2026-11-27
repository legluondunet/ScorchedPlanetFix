/* SPDX-License-Identifier: GPL-3.0-or-later
 * First, conservative native RGB565 publication pilot. All four buffers
 * must be distinct, nonoverlapping, and valid for n pixels. Single owner
 * thread, no guest callbacks or writes between comparison and publication.
 * CPU remains authoritative for coverage and any larger color difference.
 */
#ifndef MSI_GPU_PUBLISH_H
#define MSI_GPU_PUBLISH_H
#include "msi_gpu_compare.h"
typedef struct {unsigned copied,changed,preserved;} MsiGpuPublish;
static inline int msi_gpu_publish_verified(MsiGpuPublish *r,
    uint16_t *live,const uint16_t *before,const uint16_t *cpu,const uint16_t *gpu,
    unsigned n,const MsiGpuCompare *stats){
    *r=(MsiGpuPublish){0};
    if(!live||!before||!cpu||!gpu||live==before||live==cpu||live==gpu||
       before==cpu||before==gpu||cpu==gpu||!msi_gpu_compare_pass(stats,n))return 0;
    /* All-or-nothing stale check BEFORE any writes. */
    for(unsigned i=0;i<n;i++)if(live[i]!=cpu[i])return 0;
    for(unsigned i=0;i<n;i++){
        if(cpu[i]==before[i]||gpu[i]==before[i]||msi_gpu_compare_channel(cpu[i],gpu[i])>1u){
            if(cpu[i]!=gpu[i])r->preserved++;
            continue;
        }
        live[i]=gpu[i];r->copied++;r->changed+=cpu[i]!=gpu[i];
    }
    return 1;
}
#endif
