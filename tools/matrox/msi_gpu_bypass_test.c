/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "msi_gpu_bypass.h"
int main(void){
    MsiGpuDrawBatch b={0};b.count=1;b.draws[0].flags=73;
    b.draws[0].bounds=(MsiGpuRect){0,0,3,3};
    uint16_t before[16],gpu[16],alt[16],live[16];
    uint8_t under[16];MsiGpuBypass r;
    for(unsigned i=0;i<16;i++){live[i]=before[i]=gpu[i]=0x1234;alt[i]=0xedcb;under[i]=3;}
    gpu[0]=alt[0]=0x1234;gpu[1]=alt[1]=0x5555;
    live[15]=7;
    assert(!msi_gpu_bypass_commit(&r,4,4,&b,live,under,before,gpu,alt));
    assert(live[1]==0x1234&&under[0]==3&&under[1]==3);
    live[15]=before[15];gpu[15]=alt[15]=0x1111;
    assert(!msi_gpu_bypass_commit(&r,4,4,&b,live,under,before,gpu,alt)&&under[1]==3);
    gpu[15]=before[15];alt[15]=1;
    assert(!msi_gpu_bypass_commit(&r,4,4,&b,live,under,before,gpu,alt)&&under[1]==3);
    alt[15]=0xedcb;
    assert(msi_gpu_bypass_commit(&r,4,4,&b,live,under,before,gpu,alt));
    assert(r.written==2&&r.changed==1&&live[1]==0x5555&&under[0]==2&&under[1]==2&&under[2]==3);
    for(unsigned i=0;i<16;i++){live[i]=before[i];under[i]=3;}
    b.draws[0].flags=9;
    assert(msi_gpu_bypass_commit(&r,4,4,&b,live,under,before,gpu,alt)&&under[0]==1);
    b.count=2;
    assert(!msi_gpu_bypass_commit(&r,4,4,&b,live,under,before,gpu,alt));
    puts("MSI_GPU_BYPASS_PASS");return 0;
}
