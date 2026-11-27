/* SPDX-License-Identifier: GPL-3.0-or-later
 * Sparse native shadow diagnostics: no rendering, allocation, file or OS calls.
 * One owner thread. Every admitted attempt gets exactly one terminal outcome.
 */
#ifndef MSI_GPU_SHADOW_SAMPLER_H
#define MSI_GPU_SHADOW_SAMPLER_H
#define GPU_SHADOW_SAMPLE_INTERVAL 120u
#define GPU_SHADOW_SAMPLE_LIMIT 24u
typedef struct {
    unsigned sampled,empty,passed,differences,errors,skipped,disabled;
    unsigned pending,last_frame;
} MsiGpuShadowSampler;
enum {MSI_SHADOW_ERROR=-1,MSI_SHADOW_SKIP=0,MSI_SHADOW_PASS=1,
      MSI_SHADOW_DIFFERENCES=2,MSI_SHADOW_EMPTY=3};
static inline void msi_gpu_shadow_sampler_finish(MsiGpuShadowSampler *s,int outcome){
    if(!s->pending)return;
    s->pending=0;
    switch(outcome){
        case MSI_SHADOW_PASS:s->passed++;break;
        case MSI_SHADOW_EMPTY:s->empty++;break;
        case MSI_SHADOW_SKIP:s->skipped++;break;
        case MSI_SHADOW_DIFFERENCES:s->differences++;s->disabled=1;break;
        default:s->errors++;s->disabled=1;break;
    }
}
static inline int msi_gpu_shadow_sampler_begin(MsiGpuShadowSampler *s,
    unsigned frame,int enabled,int manual,int nested){
    /* A repeated start or teardown cancels a prior unfinished attempt. */
    msi_gpu_shadow_sampler_finish(s,MSI_SHADOW_SKIP);
    if(!enabled||manual||nested||s->disabled||s->passed>=GPU_SHADOW_SAMPLE_LIMIT||
       !frame||frame%GPU_SHADOW_SAMPLE_INTERVAL||s->last_frame==frame)return 0;
    s->last_frame=frame;s->sampled++;s->pending=1;return 1;
}
static inline int msi_gpu_shadow_sampler_counts_ok(const MsiGpuShadowSampler *s){
    return s->sampled==s->empty+s->passed+s->differences+s->errors+s->skipped+s->pending;
}
#endif
