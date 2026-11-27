/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include "msi_gpu_texel.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"decode test line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void set(uint8_t *p,unsigned i,unsigned c){p[2*i]=(uint8_t)c;p[2*i+1]=(uint8_t)(c>>8);}
int main(void){
    uint8_t lut[512]={0},src[8]={3,0,4,5};
    uint32_t out[4]={0};
    MsiGpuTexture s={2,2,8,1,0,0xffff,1,0,0};
    set(lut,3,0xf800);set(lut,0,0x07e0);set(lut,4,0x001f);set(lut,5,0xffff);
    CHECK(msi_gpu_decode_world(&s,src,4,lut,sizeof(lut),out,4));
    CHECK(out[0]==0xf800&&out[1]==0x307e0&&out[2]==0x001f&&out[3]==0xffff);
    s.index_key=0;CHECK(msi_gpu_decode_world(&s,src,4,lut,sizeof(lut),out,4));CHECK(out[1]==0x07e0);
    set(lut,0,0x00ff);s.blue_key=1;CHECK(msi_gpu_decode_world(&s,src,4,lut,sizeof(lut),out,4));CHECK(out[1]==0x300ff);
    s.transparent=0;CHECK(msi_gpu_decode_world(&s,src,4,lut,sizeof(lut),out,4));CHECK(out[1]==0x00ff);
    s.transparent=1;s.blue_key=0;s.index_key=1;s.width=s.height=128;
    CHECK(msi_gpu_world_texel(0,1,&s)==0x20000);
    CHECK(msi_gpu_world_texel(0,241,&s)==0x30000);
    s.icon01_match=1;CHECK(msi_gpu_world_texel(0,241,&s)==0x20000);
    CHECK(msi_gpu_world_texel(0,0,&s)==0x30000);
    CHECK(msi_gpu_world_texel(0,2,&s)==0x30000);
    s.width=127;CHECK(msi_gpu_world_texel(0,1,&s)==0x30000);
    s.width=2;s.height=2;s.planes=4;s.icon01_match=0;
    uint8_t packed[2]={0x03,0x54};set(lut,0,0);
    CHECK(msi_gpu_decode_world(&s,packed,2,lut,32,out,4));
    CHECK(out[0]==0xf800&&out[1]==0x30000&&out[2]==0x001f&&out[3]==0xffff);
    s.planes=16;s.key=0x07e0;
    uint8_t direct[8]={0x00,0xf8,0xe0,0x07,0x1f,0,0xff,0xff};
    CHECK(msi_gpu_decode_world(&s,direct,8,NULL,0,out,4));
    CHECK(out[0]==0xf800&&out[1]==0x307e0&&out[2]==0x001f&&out[3]==0xffff);
    s.key=0;s.mask=0;CHECK(msi_gpu_world_texel(0xffff,2,&s)==0x3ffff);
    out[0]=0x12345678;
    CHECK(!msi_gpu_decode_world(&s,direct,7,NULL,0,out,4));CHECK(out[0]==0x12345678);
    CHECK(!msi_gpu_decode_world(&s,direct,8,NULL,0,out,3));CHECK(out[0]==0x12345678);
    s.planes=8;CHECK(!msi_gpu_decode_world(&s,src,4,lut,511,out,4));
    s.planes=4;CHECK(!msi_gpu_decode_world(&s,packed,2,lut,31,out,4));
    s.planes=7;CHECK(!msi_gpu_decode_world(&s,src,4,lut,512,out,4));
    s.planes=16;s.width=s.height=2048;CHECK(!msi_gpu_decode_world(&s,direct,8,NULL,0,out,4));
    s.width=0;CHECK(!msi_gpu_decode_world(&s,direct,8,NULL,0,out,4));
    /* odd texel count: low nibble first, ignore unused high nibble */
    s=(MsiGpuTexture){3,1,4,0,0,0,0,0,0};
    CHECK(msi_gpu_decode_world(&s,packed,2,lut,32,out,3));
    CHECK(out[0]==0xf800&&out[1]==0&&out[2]==0x001f);
    /* Compare every output against the unchanged scalar expression.
     * All indices recur across each 128x128 texture, including special
     * black entries 1/241, blue key, index masks and full RGB565 masks. */
    static uint8_t large[128u*128u];
    static uint32_t expanded[128u*128u];
    for(unsigned i=0;i<sizeof(large);i++)large[i]=(uint8_t)(i*73u+(i>>8));
    for(unsigned i=0;i<256u;i++)set(lut,i,(i*257u)^0x5a5au);
    set(lut,0,0);set(lut,1,0);set(lut,241,0);set(lut,5,0xff);
    const unsigned keys[]={0u,5u,0xffu,0x5a5au,0xffffu};
    const unsigned masks[]={0u,0xffu,0xffffu,0x00f0u};
    for(unsigned bits=0;bits<16u;bits++)for(unsigned k=0;k<5u;k++)for(unsigned m=0;m<4u;m++){
        s=(MsiGpuTexture){128,128,8,bits&1u,keys[k],masks[m],(bits>>1)&1u,(bits>>2)&1u,(bits>>3)&1u};
        CHECK(msi_gpu_decode_world(&s,large,sizeof(large),lut,sizeof(lut),expanded,sizeof(large)));
        for(unsigned i=0;i<sizeof(large);i++){
            unsigned index=large[i];
            CHECK(expanded[i]==msi_gpu_world_texel(msi_gpu_le16(lut+2u*index),index,&s));
        }
    }
    /* Threshold and invalid inputs: no writes on insufficient capacity. */
    s=(MsiGpuTexture){256,1,8,1,0,0xffff,1,0,0};
    expanded[256]=0x12345678;
    CHECK(msi_gpu_decode_world(&s,large,256,lut,512,expanded,256));
    for(unsigned i=0;i<256u;i++)CHECK(expanded[i]==msi_gpu_world_texel(msi_gpu_le16(lut+2u*large[i]),large[i],&s));
    CHECK(expanded[256]==0x12345678);
    expanded[0]=0x12345678;
    CHECK(!msi_gpu_decode_world(&s,large,255,lut,512,expanded,256)&&expanded[0]==0x12345678);
    CHECK(!msi_gpu_decode_world(&s,large,256,lut,511,expanded,256)&&expanded[0]==0x12345678);
    CHECK(!msi_gpu_decode_world(&s,large,256,lut,512,expanded,255)&&expanded[0]==0x12345678);
    puts("MSI_GPU_TEXTURE_DECODE_PASS");return 0;
}
