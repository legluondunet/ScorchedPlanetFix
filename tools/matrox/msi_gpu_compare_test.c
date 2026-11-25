/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "msi_gpu_compare.h"
int main(void){
    MsiGpuDrawBatch b={0};MsiGpuCompare r;
    uint16_t before[64]={0},cpu[64]={0},gpu[64]={0};
    b.count=1;b.draws[0].bounds=(MsiGpuRect){0,0,6,6};b.draws[0].flags=121;
    b.vertices[1].xy[0]=6.f;b.vertices[2].xy[1]=6.f;
    cpu[0]=gpu[0]=0xf800;
    cpu[9]=0x0020;gpu[9]=0x0040;
    cpu[26]=0xf800; /* center 2.5,3.5 lies on diagonal */
    cpu[18]=0xf800;gpu[18]=0x001f;
    gpu[63]=0x001f;
    msi_gpu_compare(&r,&b,8,8,before,cpu,gpu);
    assert(r.exact==60&&r.active==5&&r.active_exact==1&&r.one==1&&r.edge==1&&r.bad==1&&r.outside==1);
    assert(r.cpu_only==1&&r.gpu_only==1&&r.both_changed==3);
    assert(r.examples[0].x==2&&r.examples[0].y==2&&r.examples[0].expected==0xf800&&r.examples[0].actual==0x001f);
    assert(r.perspective==1&&r.bilinear==1&&r.modulated==1&&r.dithered==1&&r.keyed==1&&!r.affine_nearest);
    assert(msi_gpu_compare_counts_ok(&r,64)&&!msi_gpu_compare_pass(&r,64));
    gpu[18]=cpu[18];gpu[63]=0;
    msi_gpu_compare(&r,&b,8,8,before,cpu,gpu);
    assert(r.active==4&&r.active_exact==2&&r.one==1&&r.edge==1&&msi_gpu_compare_pass(&r,64));
    msi_gpu_compare(&r,&b,8,8,before,before,before);
    assert(r.exact==64&&r.active==0&&msi_gpu_compare_counts_ok(&r,64)&&!msi_gpu_compare_pass(&r,64));
    puts("MSI_GPU_COMPARE_PASS");return 0;
}
