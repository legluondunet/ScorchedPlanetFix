/* SPDX-License-Identifier: GPL-3.0-or-later
 * Ordered conservative wave planner. Bounds are clipped destination pixels,
 * half-open [x0,x1) x [y0,y1), supplied by caller. No reordering, no allocation.
 * Texture/state snapshots remain caller responsibility. Explicit guest
 * barriers must flush pending work; this helper does not implement them.
 */
#ifndef MSI_GPU_BATCH_H
#define MSI_GPU_BATCH_H
#include <stdint.h>
#define MSI_GPU_WAVE_MAX 128u
typedef struct {uint32_t x0,y0,x1,y1;} MsiGpuRect;
typedef struct {
    unsigned count,needs_prior;
    MsiGpuRect rects[MSI_GPU_WAVE_MAX];
} MsiGpuWave;
enum {MSI_GPU_WAVE_SKIP=0,MSI_GPU_WAVE_ACCEPT=1,MSI_GPU_WAVE_FLUSH=2};
static int msi_gpu_rect_overlap(MsiGpuRect a,MsiGpuRect b){
    return a.x0<b.x1&&b.x0<a.x1&&a.y0<b.y1&&b.y0<a.y1;
}
static void msi_gpu_wave_reset(MsiGpuWave *w){w->count=0;w->needs_prior=0;}
/* FLUSH does not mutate state. Flush, reset and retry SAME primitive.
 * Opaque draws can overlap in one ordered pass. Prior-dependent draws can
 * share one pre-wave snapshot only if their conservative bounds are disjoint.
 * No mixing of these two kinds in a wave. Never group across guest barriers. */
static int msi_gpu_wave_push(MsiGpuWave *w,MsiGpuRect r,unsigned needs_prior){
    if(r.x0>=r.x1||r.y0>=r.y1)return MSI_GPU_WAVE_SKIP;
    needs_prior=!!needs_prior;
    if(w->count){
        if(w->count>=MSI_GPU_WAVE_MAX||w->needs_prior!=needs_prior)return MSI_GPU_WAVE_FLUSH;
        if(needs_prior)for(unsigned i=0;i<w->count;i++)
            if(msi_gpu_rect_overlap(r,w->rects[i]))return MSI_GPU_WAVE_FLUSH;
    }
    w->needs_prior=needs_prior;w->rects[w->count++]=r;return MSI_GPU_WAVE_ACCEPT;
}
#endif
