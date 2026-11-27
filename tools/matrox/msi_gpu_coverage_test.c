/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "msi_gpu_coverage.h"
int main(void){
    MsiGpuDrawBatch b={0};b.count=1;
    b.draws[0].bounds=(MsiGpuRect){0,0,4,4};
    b.vertices[1].xy[0]=4;b.vertices[2].xy[1]=4;
    uint16_t before[25]={0},cpu[25]={0},gpu[25]={0},alt[25];
    uint8_t mask[25]={0};MsiGpuCoverage r;
    for(unsigned i=0;i<25;i++)alt[i]=65535u;
    /* Opaque writes that match the original color must still count. */
    mask[0]=1;alt[0]=0;
    mask[1]=1;cpu[1]=gpu[1]=alt[1]=0x1234;
    /* CPU-only and GPU-only diagonal edge pixels. */
    mask[3]=1;cpu[3]=0x1234;gpu[7]=alt[7]=0x1234;
    msi_gpu_coverage(&r,&b,5,5,mask,before,cpu,gpu,alt);
    assert(msi_gpu_coverage_pass(&r)&&r.cpu_written==3&&r.gpu_written==3&&
        r.both==2&&r.same_color==1&&r.edge==2);
    mask[6]=1;
    msi_gpu_coverage(&r,&b,5,5,mask,before,cpu,gpu,alt);
    assert(!msi_gpu_coverage_pass(&r)&&r.bad==1);
    mask[6]=0;gpu[24]=alt[24]=1;
    msi_gpu_coverage(&r,&b,5,5,mask,before,cpu,gpu,alt);
    assert(!msi_gpu_coverage_pass(&r)&&r.outside==1);
    gpu[24]=0;alt[24]=123;
    msi_gpu_coverage(&r,&b,5,5,mask,before,cpu,gpu,alt);
    assert(!msi_gpu_coverage_pass(&r)&&r.invalid==1);
    uint8_t categories[4]={2,0,2,0};
    uint16_t a[4]={0,123,65535,22},tagged_alt[4]={0,456,65535,33};
    uint32_t tagged[4]={0,65536u,65535u,65536u};
    b.count=1;b.draws[0].flags=73;
    MsiGpuTaggedCheck tagged_check;
    msi_gpu_tagged_check(&tagged_check,&b,4,categories,a,tagged_alt,tagged);
    assert(msi_gpu_tagged_pass(&tagged_check,4)&&tagged_check.written==2&&tagged_check.provenance_checked==2);
    tagged[0]=1;
    msi_gpu_tagged_check(&tagged_check,&b,4,categories,a,tagged_alt,tagged);
    assert(!msi_gpu_tagged_pass(&tagged_check,4)&&tagged_check.color_diff==1);
    tagged[0]=65536u;
    msi_gpu_tagged_check(&tagged_check,&b,4,categories,a,tagged_alt,tagged);
    assert(!msi_gpu_tagged_pass(&tagged_check,4)&&tagged_check.coverage_diff==1);
    tagged[0]=0;categories[0]=1;
    msi_gpu_tagged_check(&tagged_check,&b,4,categories,a,tagged_alt,tagged);
    assert(!msi_gpu_tagged_pass(&tagged_check,4)&&tagged_check.provenance_diff==1);
    b.count=2;b.draws[1].flags=9;
    msi_gpu_tagged_check(&tagged_check,&b,4,categories,a,tagged_alt,tagged);
    assert(msi_gpu_tagged_pass(&tagged_check,4)&&!tagged_check.homogeneous&&!tagged_check.provenance_checked);
    tagged[0]=65537u;
    msi_gpu_tagged_check(&tagged_check,&b,4,categories,a,tagged_alt,tagged);
    assert(!msi_gpu_tagged_pass(&tagged_check,4)&&tagged_check.invalid==1);
    puts("MSI_GPU_TAGGED_CHECK_PASS");
    puts("MSI_GPU_COVERAGE_PASS");return 0;
}
