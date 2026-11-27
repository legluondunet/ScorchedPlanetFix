/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "msi_gpu_defer_admit.h"
#include "msi_gpu_cpu_replay.h"
static MsiGpuDrawBatch b;
static uint16_t before[640u*400u],out[640u*400u];
static uint8_t written[640u*400u],raw[1024];
static uint32_t decoded[4];
int main(void){
    assert(msi_gpu_defer_overlap(100,1,100,100));
    assert(msi_gpu_defer_overlap(99,2,100,100));
    assert(msi_gpu_defer_overlap(199,1,100,100));
    assert(!msi_gpu_defer_overlap(200,1,100,100));
    assert(!msi_gpu_defer_overlap(99,1,100,100));
    assert(!msi_gpu_defer_overlap(100,0,100,100));
    assert(msi_gpu_defer_overlap(UINTPTR_MAX-1,1,UINTPTR_MAX-2,2));
    MsiGpuVertex v[3]={{{10,10},{-128,128},.0001f,{255,255,255}},
        {{60,11},{128,-128},.0064f,{128,255,0}},
        {{11,60},{0,0},.001f,{0,128,255}}};
    MsiGpuRect r={10,10,61,61};
    assert(msi_gpu_defer_draw_admissible(v,r,121));
    assert(!msi_gpu_defer_draw_admissible(v,r,120));
    assert(!msi_gpu_defer_draw_admissible(v,r,249));
    assert(!msi_gpu_defer_draw_admissible(v,(MsiGpuRect){0,0,641,1},121));
    v[0].uv[0]=NAN;assert(!msi_gpu_defer_draw_admissible(v,r,121));v[0].uv[0]=-128;
    v[1].iw=.0065f;assert(!msi_gpu_defer_draw_admissible(v,r,121));v[1].iw=.0064f;
    v[0].iw=0;assert(!msi_gpu_defer_draw_admissible(v,r,121));v[0].iw=.0001f;
    v[2].xy[0]=60;v[2].xy[1]=11;assert(!msi_gpu_defer_draw_admissible(v,r,121));
    v[2].xy[0]=11;v[2].xy[1]=60;
    b.snapshots.raw=raw;b.snapshots.raw_capacity=sizeof(raw);
    b.snapshots.decoded=decoded;b.snapshots.decoded_capacity=4;
    uint8_t source[4]={0,1,1,0},lut[512]={0};
    lut[2]=0xff;lut[3]=0xff;
    MsiGpuTexture tex={2,2,8,1,0,65535,0,0,0};
    assert(msi_gpu_draw_batch_push(&b,v,r,121,&tex,source,4,lut,512)==MSI_GPU_DRAW_ACCEPT);
    assert(msi_gpu_defer_batch_admissible(&b));
    assert(msi_gpu_cpu_replay(&b,640,400,before,out,written));
    unsigned n=0;for(unsigned i=0;i<640u*400u;i++)n+=written[i]!=0;assert(n>0);
    b.draws[0].texture_id=1;assert(!msi_gpu_defer_batch_admissible(&b));b.draws[0].texture_id=0;
    decoded[0]|=0x40000u;assert(!msi_gpu_defer_batch_admissible(&b));decoded[0]&=0x3ffffu;
    b.snapshots.entries[0].decoded_count=3;assert(!msi_gpu_defer_batch_admissible(&b));
    puts("MSI_GPU_DEFER_ADMISSION_PASS");return 0;
}
