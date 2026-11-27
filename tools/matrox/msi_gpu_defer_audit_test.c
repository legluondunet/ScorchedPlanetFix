/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "msi_gpu_defer_audit.h"
int main(void){
    MsiGpuDeferAudit a={0};
    assert(msi_gpu_defer_pointer(1,2,1,2)==MSI_DEFER_MAPPED);
    assert(msi_gpu_defer_pointer(0xdeadbeef,0xdeadbeef,1,2)==MSI_DEFER_INVALIDATED);
    assert(msi_gpu_defer_pointer(0xdeadbeef,2,1,2)==MSI_DEFER_UNKNOWN);
    assert(msi_gpu_defer_pointer(0,0,0,0)==MSI_DEFER_UNKNOWN);
    msi_gpu_defer_candidate(&a,MSI_DEFER_MAPPED,1,1);
    msi_gpu_defer_candidate(&a,MSI_DEFER_UNKNOWN,1,1);
    for(unsigned i=0;i<33u;i++)msi_gpu_defer_candidate(&a,MSI_DEFER_INVALIDATED,1,1);
    assert(a.split_limit==1&&a.pending==1&&a.max_group==32);
    msi_gpu_defer_candidate(&a,MSI_DEFER_INVALIDATED,0,1);
    assert(a.split_key==1&&a.pending==1);
    msi_gpu_defer_candidate(&a,MSI_DEFER_INVALIDATED,0,1);
    msi_gpu_defer_candidate(&a,MSI_DEFER_INVALIDATED,0,0);
    assert(a.barriers==1&&a.pending==0&&a.groups==3&&a.multi_groups==2&&a.multi_draws==34);
    assert(a.eligible==37&&a.invalidated==35&&a.excluded==1&&msi_gpu_defer_counts_ok(&a));
    msi_gpu_defer_candidate(&a,MSI_DEFER_INVALIDATED,1,1);
    msi_gpu_defer_barrier(&a);msi_gpu_defer_barrier(&a);
    assert(a.barriers==2&&msi_gpu_defer_counts_ok(&a));
    puts("MSI_GPU_DEFER_AUDIT_PASS");return 0;
}
