/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "msi_gpu_draw_batch.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"draw recorder line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
_Static_assert(sizeof(MsiGpuVertex)==32,"GPU vertex ABI");
static MsiGpuDrawBatch batch;
static uint8_t raw[4096],source[4]={1,0,1,1},palette[512];
static uint32_t decoded[128];
static MsiGpuVertex v[3]={{{0,0},{0,0},1,{255,255,255}},{{64,0},{1,0},2,{255,255,255}},{{0,64},{0,1},3,{255,255,255}}};
static MsiGpuTexture state={2,2,8,1,0,65535,1,0,0};
static int push(unsigned flags){return msi_gpu_draw_batch_push(&batch,v,(MsiGpuRect){0,0,64,64},flags,&state,source,sizeof(source),palette,sizeof(palette));}
int main(void){
    batch.snapshots.raw=raw;batch.snapshots.raw_capacity=sizeof(raw);
    batch.snapshots.decoded=decoded;batch.snapshots.decoded_capacity=128;
    msi_gpu_draw_batch_reset(&batch);palette[2]=0x1f;
    CHECK(push(65)==MSI_GPU_DRAW_ACCEPT);
    v[0].xy[0]=9;palette[2]=0;palette[3]=0xf8;
    CHECK(push(64)==MSI_GPU_DRAW_ACCEPT);
    CHECK(batch.count==2&&batch.misses==2&&batch.vertices[0].xy[0]==0&&batch.vertices[3].xy[0]==9);
    CHECK(batch.vertices[1].iw==2&&batch.vertices[4].iw==1);
    CHECK(decoded[0]==0x001f&&decoded[4]==0xf800);
    CHECK(push(64)==MSI_GPU_DRAW_ACCEPT&&batch.hits==1);
    unsigned count=batch.count,unique=batch.snapshots.count;
    CHECK(push(256)==MSI_GPU_DRAW_INVALID&&batch.count==count&&batch.snapshots.count==unique);
    CHECK(push(0)==MSI_GPU_DRAW_INVALID);
    v[0].iw=NAN;CHECK(push(65)==MSI_GPU_DRAW_INVALID);CHECK(push(64)==MSI_GPU_DRAW_ACCEPT);
    v[0].iw=1;v[0].uv[0]=NAN;CHECK(push(64)==MSI_GPU_DRAW_INVALID);v[0].uv[0]=0;
    msi_gpu_draw_batch_reset(&batch);
    for(unsigned i=0;i<128;i++)CHECK(push(64)==MSI_GPU_DRAW_ACCEPT);
    CHECK(batch.count==128&&batch.hits==127&&batch.misses==1);
    CHECK(push(64)==MSI_GPU_DRAW_FLUSH&&batch.count==128);
    msi_gpu_draw_batch_reset(&batch);batch.snapshots.decoded_capacity=4;
    CHECK(push(64)==MSI_GPU_DRAW_ACCEPT);palette[3]=0x7c;
    CHECK(push(64)==MSI_GPU_DRAW_FLUSH&&batch.count==1&&batch.snapshots.count==1);
    puts("MSI_GPU_DRAW_BATCH_PASS");return 0;
}
