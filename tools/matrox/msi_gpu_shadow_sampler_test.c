/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "msi_gpu_shadow_sampler.h"
int main(void){
    MsiGpuShadowSampler s={0};
    assert(!msi_gpu_shadow_sampler_begin(&s,0,1,0,0));
    assert(!msi_gpu_shadow_sampler_begin(&s,119,1,0,0));
    assert(!msi_gpu_shadow_sampler_begin(&s,120,0,0,0));
    assert(!msi_gpu_shadow_sampler_begin(&s,120,1,1,0));
    assert(!msi_gpu_shadow_sampler_begin(&s,120,1,0,1));
    assert(msi_gpu_shadow_sampler_begin(&s,120,1,0,0));
    assert(!msi_gpu_shadow_sampler_begin(&s,120,1,0,0));
    assert(s.sampled==1&&s.skipped==1&&!s.pending);
    assert(msi_gpu_shadow_sampler_begin(&s,240,1,0,0));
    msi_gpu_shadow_sampler_finish(&s,MSI_SHADOW_EMPTY);
    assert(s.empty==1&&!s.passed);
    for(unsigned i=0;i<GPU_SHADOW_SAMPLE_LIMIT;i++){
        assert(msi_gpu_shadow_sampler_begin(&s,360+i*120,1,0,0));
        msi_gpu_shadow_sampler_finish(&s,MSI_SHADOW_PASS);
        msi_gpu_shadow_sampler_finish(&s,MSI_SHADOW_ERROR);
    }
    assert(!msi_gpu_shadow_sampler_begin(&s,360+GPU_SHADOW_SAMPLE_LIMIT*120,1,0,0));
    assert(s.passed==24&&s.sampled==26&&!s.errors&&!s.disabled&&msi_gpu_shadow_sampler_counts_ok(&s));
    s=(MsiGpuShadowSampler){0};
    assert(msi_gpu_shadow_sampler_begin(&s,120,1,0,0));
    msi_gpu_shadow_sampler_finish(&s,MSI_SHADOW_DIFFERENCES);
    assert(s.disabled&&s.differences==1&&!msi_gpu_shadow_sampler_begin(&s,240,1,0,0));
    assert(msi_gpu_shadow_sampler_counts_ok(&s));
    s=(MsiGpuShadowSampler){0};
    assert(msi_gpu_shadow_sampler_begin(&s,120,1,0,0));
    msi_gpu_shadow_sampler_finish(&s,MSI_SHADOW_ERROR);
    assert(s.disabled&&s.errors==1&&!msi_gpu_shadow_sampler_begin(&s,240,1,0,0));
    assert(msi_gpu_shadow_sampler_counts_ok(&s));
    puts("MSI_GPU_SHADOW_SAMPLER_PASS");return 0;
}
