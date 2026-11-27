/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "msi_gpu_batch.h"
#include "msi_gpu_shadow_state.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d failed: %s\n",__LINE__,#x);exit(1);}}while(0)
#define SIDE 32u
#define COUNT (SIDE*SIDE)
static uint32_t rng=0x7142ac9u;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
typedef struct {MsiGpuRect r;unsigned alpha,value;} Draw;
static unsigned apply(unsigned old,Draw d){return d.alpha?(old+d.value+1u)/2u:d.value;}
static void paint(unsigned *dst,const unsigned *snapshot,Draw d){
    for(unsigned y=d.r.y0;y<d.r.y1;y++)for(unsigned x=d.r.x0;x<d.r.x1;x++){
        unsigned i=y*SIDE+x;dst[i]=apply(snapshot[i],d);
    }
}
static void flush(MsiGpuWave *w,Draw *queue,unsigned *dst,unsigned *waves){
    if(!w->count)return;
    unsigned before[COUNT];memcpy(before,dst,sizeof(before));
    for(unsigned i=0;i<w->count;i++)paint(dst,w->needs_prior?before:dst,queue[i]);
    (*waves)++;msi_gpu_wave_reset(w);
}
int main(void){
    MsiGpuWave w={0};MsiGpuRect a={0,0,4,4},b={4,0,8,4},c={3,3,5,5};
    CHECK(msi_gpu_wave_push(&w,a,1)==MSI_GPU_WAVE_ACCEPT);
    CHECK(msi_gpu_wave_push(&w,b,1)==MSI_GPU_WAVE_ACCEPT);
    CHECK(msi_gpu_wave_push(&w,c,1)==MSI_GPU_WAVE_FLUSH&&w.count==2);
    CHECK(msi_gpu_wave_push(&w,a,0)==MSI_GPU_WAVE_FLUSH&&w.count==2);
    CHECK(msi_gpu_wave_push(&w,(MsiGpuRect){5,5,5,6},1)==MSI_GPU_WAVE_SKIP&&w.count==2);
    msi_gpu_wave_reset(&w);
    for(unsigned i=0;i<MSI_GPU_WAVE_MAX;i++)CHECK(msi_gpu_wave_push(&w,a,0)==MSI_GPU_WAVE_ACCEPT);
    CHECK(msi_gpu_wave_push(&w,b,0)==MSI_GPU_WAVE_FLUSH);
    msi_gpu_wave_reset(&w);
    unsigned sequential[COUNT]={0},batched[COUNT]={0};Draw queue[MSI_GPU_WAVE_MAX];unsigned waves=0,accepted=0;
    for(unsigned i=0;i<20000;i++){
        unsigned x=next()%SIDE,y=next()%SIDE,x1=x+1+next()%8,y1=y+1+next()%8;
        if(x1>SIDE)x1=SIDE;if(y1>SIDE)y1=SIDE;
        Draw d={{x,y,x1,y1},next()%2,next()%65536u};
        paint(sequential,sequential,d);
        int result=msi_gpu_wave_push(&w,d.r,d.alpha);
        if(result==MSI_GPU_WAVE_FLUSH){flush(&w,queue,batched,&waves);result=msi_gpu_wave_push(&w,d.r,d.alpha);}
        CHECK(result==MSI_GPU_WAVE_ACCEPT);queue[w.count-1]=d;accepted++;
        if(i%97u==0){flush(&w,queue,batched,&waves);CHECK(memcmp(sequential,batched,sizeof(batched))==0);}
    }
    flush(&w,queue,batched,&waves);CHECK(memcmp(sequential,batched,sizeof(batched))==0);
    printf("MSI_GPU_WAVE_PASS draws=%u waves=%u\n",accepted,waves);
    /* Shadow once: textured base, first shadow, duplicate, fresh base,
     * keyed discard, flat/HUD reset, no-base shadow. */
    uint32_t v=msi_gpu_shadow_write(0,0xffff,100,1,0,0,4,0);CHECK(v==0x1ffff);
    v=msi_gpu_shadow_write(v,0x0841,100,0,0,0,4,0);CHECK(v==0x38430);
    CHECK(msi_gpu_shadow_write(v,0x0841,100,0,0,0,4,0)==v);
    CHECK(msi_gpu_shadow_write(v,0xf800,100,1,0,1,4,1)==v);
    v=msi_gpu_shadow_write(v,0xf800,100,1,0,1,4,0);CHECK(v==0x2f800);
    v=msi_gpu_shadow_write(v,0x0841,100,0,0,0,4,0);CHECK((v>>16)==3);
    v=msi_gpu_shadow_write(v,0x0841,100,0,1,0,4,0);CHECK(v==0x0841);
    CHECK(msi_gpu_shadow_write(v,0x0841,100,0,0,0,4,0)==0x0841);
    CHECK(msi_gpu_shadow_write(0x1ffff,0x0841,100,0,0,0,0,0)==0x0841);
    CHECK(msi_gpu_shadow_write(0xffff,0x0841,100,0,0,0,1,0)==0x8430);
    CHECK(msi_gpu_shadow_write(0xffff,0x0841,100,0,0,0,2,0)==0x0841);
    CHECK(msi_gpu_shadow_write(0x2ffff,0x0841,100,0,0,0,2,0)==0x8430);
    CHECK(msi_gpu_shadow_write(0x2ffff,0xf800,0,1,0,1,4,0)==0x2ffff);
    puts("MSI_GPU_SHADOW_STATE_PASS");return 0;
}
