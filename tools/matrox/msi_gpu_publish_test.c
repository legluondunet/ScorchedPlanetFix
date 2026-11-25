/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "msi_gpu_publish.h"
int main(void){
    MsiGpuDrawBatch b={0};b.count=1;
    b.draws[0].bounds=(MsiGpuRect){0,0,4,4};
    b.vertices[0].xy[0]=0;b.vertices[0].xy[1]=0;
    b.vertices[1].xy[0]=4;b.vertices[1].xy[1]=0;
    b.vertices[2].xy[0]=0;b.vertices[2].xy[1]=4;
    uint16_t before[16]={0},cpu[16]={0},gpu[16]={0},live[16]={0};
    cpu[0]=gpu[0]=0x1234;cpu[1]=0x1234;gpu[1]=0x1235;
    /* Pixel centers on the triangle diagonal: retain CPU coverage/color. */
    cpu[3]=0x1234;gpu[6]=0x1234;cpu[9]=0x1234;gpu[9]=0x5678;
    for(unsigned i=0;i<16;i++)live[i]=cpu[i];
    MsiGpuCompare c;MsiGpuPublish r;
    msi_gpu_compare(&c,&b,4,4,before,cpu,gpu);
    assert(msi_gpu_compare_pass(&c,16));
    assert(msi_gpu_publish_verified(&r,live,before,cpu,gpu,16,&c));
    assert(r.copied==2&&r.changed==1&&r.preserved==3);
    assert(live[0]==gpu[0]&&live[1]==gpu[1]&&live[3]==cpu[3]&&live[6]==cpu[6]&&live[9]==cpu[9]);
    for(unsigned i=0;i<16;i++)live[i]=cpu[i];
    live[15]=0xbeef;assert(!msi_gpu_publish_verified(&r,live,before,cpu,gpu,16,&c));
    assert(live[1]==cpu[1]&&live[15]==0xbeef&&!r.copied);
    live[15]=cpu[15];c.bad=1;
    assert(!msi_gpu_publish_verified(&r,live,before,cpu,gpu,16,&c)&&live[1]==cpu[1]);
    c.bad=0;assert(!msi_gpu_publish_verified(&r,cpu,before,cpu,gpu,16,&c));
    puts("MSI_GPU_PUBLISH_PASS");return 0;
}
