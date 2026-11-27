/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msi_gpu_snapshot.h"
#include "msi_gpu_pilot_storage.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"snapshot line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t raw[128*(512+64)],src[64],lut[512],other[64];
static uint32_t decoded[128*64];
static MsiGpuSnapshots cache;
static void palette(unsigned i,unsigned v){lut[2*i]=(uint8_t)v;lut[2*i+1]=(uint8_t)(v>>8);}
static uint32_t value(unsigned id,unsigned i){return decoded[cache.entries[id].decoded_offset+i];}
static unsigned intern(const MsiGpuTexture *s,const uint8_t *p,int result){
    unsigned id=9999;CHECK(msi_gpu_snapshot_indexed8(&cache,s,p,64,lut,512,&id)==result);
    if(result==MSI_GPU_SNAPSHOT_FULL||result==MSI_GPU_SNAPSHOT_INVALID)CHECK(id==9999);
    return id;
}
static unsigned alloc_calls,live_allocs,fail_at;
static void *pilot_alloc(unsigned n){
    alloc_calls++;if(alloc_calls==fail_at)return NULL;
    void *p=malloc(n);if(p)live_allocs++;return p;
}
static void pilot_free(void *p){CHECK(p&&live_allocs);live_allocs--;free(p);}
static void pilot_storage_test(void){
    MsiGpuPilotStorage s={0};
    for(unsigned i=1;i<=4;i++){
        alloc_calls=0;fail_at=i;
        CHECK(msi_gpu_pilot_storage_acquire(&s,pilot_alloc,pilot_free)==0);
        CHECK(!live_allocs&&!s.before&&!s.raw&&!s.decoded&&!s.packed);
    }
    alloc_calls=0;fail_at=0;
    CHECK(msi_gpu_pilot_storage_acquire(&s,pilot_alloc,pilot_free)==1&&alloc_calls==4&&live_allocs==4);
    MsiGpuPilotStorage original=s;
    s.raw[MSI_GPU_PILOT_RAW_BYTES-1u]=0x87;
    s.decoded[MSI_GPU_PILOT_DECODED_COUNT-1u]=0x3ffffu;
    s.packed[MSI_GPU_PILOT_PACKED_COUNT-1u]=65536u;
    for(unsigned i=0;i<12;i++){
        CHECK(msi_gpu_pilot_storage_acquire(&s,pilot_alloc,pilot_free)==2);
        CHECK(s.before==original.before&&s.raw==original.raw&&s.decoded==original.decoded&&s.packed==original.packed&&alloc_calls==4);
        CHECK(s.raw[MSI_GPU_PILOT_RAW_BYTES-1u]==0x87&&s.decoded[MSI_GPU_PILOT_DECODED_COUNT-1u]==0x3ffffu&&s.packed[MSI_GPU_PILOT_PACKED_COUNT-1u]==65536u);
    }
    static uint8_t srcbuf[MSI_GPU_PILOT_NATIVE_BYTES+2u],dstbuf[MSI_GPU_PILOT_NATIVE_BYTES+2u];
    for(unsigned i=0;i<sizeof(srcbuf);i++)srcbuf[i]=(uint8_t)(i*137u+(i>>10));
    dstbuf[0]=0xc7;dstbuf[sizeof(dstbuf)-1u]=0xa5;
    msi_gpu_native_copy(dstbuf+1,srcbuf+1);
    CHECK(!memcmp(dstbuf+1,srcbuf+1,MSI_GPU_PILOT_NATIVE_BYTES));
    CHECK(dstbuf[0]==0xc7&&dstbuf[sizeof(dstbuf)-1u]==0xa5);
    msi_gpu_native_copy(s.before,srcbuf+1);
    CHECK(!memcmp(s.before,srcbuf+1,MSI_GPU_PILOT_NATIVE_BYTES));
    msi_gpu_pilot_storage_destroy(&s,pilot_free);
    msi_gpu_pilot_storage_destroy(&s,pilot_free);
    CHECK(!live_allocs&&!s.before&&!s.raw&&!s.decoded&&!s.packed);
    CHECK(msi_gpu_pilot_storage_acquire(&s,pilot_alloc,pilot_free)==1&&live_allocs==4);
    msi_gpu_pilot_storage_destroy(&s,pilot_free);CHECK(!live_allocs);
    puts("MSI_GPU_PILOT_STORAGE_PASS faults=4 reuse=12 copy=512000 guards=1");
}
static void exact_compare_test(void){
    uint8_t a[80],b[80];
    unsigned comparisons=0;
    for(unsigned ao=0;ao<4;ao++)for(unsigned bo=0;bo<4;bo++)
        for(unsigned n=0;n<=64;n++){
            memset(a,0x33,sizeof(a));memset(b,0x55,sizeof(b));
            for(unsigned k=0;k<n;k++)a[ao+k]=b[bo+k]=(uint8_t)(k*19u);
            CHECK(msi_gpu_snapshot_equal(a+ao,b+bo,n));comparisons++;
            for(unsigned k=0;k<n;k++){
                b[bo+k]^=0x80u;CHECK(!msi_gpu_snapshot_equal(a+ao,b+bo,n));comparisons++;
                b[bo+k]^=0x80u;
            }
        }
    printf("MSI_GPU_EXACT_COMPARE_PASS comparisons=%u\n",comparisons);
}
int main(void){
    exact_compare_test();
    pilot_storage_test();
    cache.raw=raw;cache.raw_capacity=sizeof(raw);cache.decoded=decoded;cache.decoded_capacity=sizeof(decoded)/sizeof(decoded[0]);
    MsiGpuTexture s={8,8,8,1,0,0xffff,1,0,0};
    memset(src,3,sizeof(src));memcpy(other,src,sizeof(src));palette(3,0xf800);palette(4,0x001f);
    unsigned a=intern(&s,src,MSI_GPU_SNAPSHOT_NEW);
    CHECK(a==0&&value(a,0)==0xf800&&intern(&s,other,MSI_GPU_SNAPSHOT_HIT)==a);
    /* Independent CLUT change without texture offset or source change. */
    palette(3,0x07e0);unsigned b=intern(&s,src,MSI_GPU_SNAPSHOT_NEW);
    CHECK(b==1&&value(a,0)==0xf800&&value(b,0)==0x07e0);
    /* Direct write to same source address must create a new snapshot. */
    src[0]=4;unsigned c=intern(&s,src,MSI_GPU_SNAPSHOT_NEW);
    CHECK(c==2&&value(c,0)==0x001f&&value(b,0)==0x07e0);
    s.key=0x001f;unsigned e=intern(&s,src,MSI_GPU_SNAPSHOT_NEW);
    CHECK(value(e,0)==0x3001f&&value(c,0)==0x001f);
    s.transparent=0;unsigned f=intern(&s,src,MSI_GPU_SNAPSHOT_NEW);
    CHECK(value(f,0)==0x001f);
    /* Simulate hash collision by forcing old metadata to match new hash.
       Exact original bytes must still prevent a false cache hit. */
    uint32_t hash=msi_gpu_snapshot_hash(lut,512,msi_gpu_snapshot_hash(src,64,2166136261u));
    cache.entries[0].hash=hash;cache.entries[0].state=s;
    CHECK(intern(&s,src,MSI_GPU_SNAPSHOT_HIT)==f);
    /* Exact hits must not depend on the cached hash metadata. Original
     * FNV hash for newly interned bytes remains unchanged. */
    CHECK(cache.entries[f].hash==hash);
    cache.entries[f].hash^=1u;
    CHECK(intern(&s,src,MSI_GPU_SNAPSHOT_HIT)==f);
    cache.entries[f].hash=hash;
    msi_gpu_snapshot_reset(&cache);s.transparent=1;s.key=0;
    for(unsigned i=0;i<128;i++){palette(3,i+1);CHECK(intern(&s,src,MSI_GPU_SNAPSHOT_NEW)==i);}
    CHECK(cache.count==128);size_t ru=cache.raw_used,du=cache.decoded_used;
    palette(3,999);intern(&s,src,MSI_GPU_SNAPSHOT_FULL);
    CHECK(cache.count==128&&cache.raw_used==ru&&cache.decoded_used==du);
    palette(3,128);CHECK(intern(&s,src,MSI_GPU_SNAPSHOT_HIT)==127);
    msi_gpu_snapshot_reset(&cache);
    cache.decoded_capacity=63;intern(&s,src,MSI_GPU_SNAPSHOT_FULL);
    CHECK(!cache.count&&!cache.raw_used&&!cache.decoded_used);
    cache.decoded_capacity=64;cache.raw_capacity=575;intern(&s,src,MSI_GPU_SNAPSHOT_FULL);
    cache.raw_capacity=576;CHECK(intern(&s,src,MSI_GPU_SNAPSHOT_NEW)==0);
    unsigned id=9999;CHECK(msi_gpu_snapshot_indexed8(&cache,&s,src,63,lut,512,&id)==MSI_GPU_SNAPSHOT_INVALID&&id==9999);
    CHECK(msi_gpu_snapshot_indexed8(&cache,&s,src,64,lut,511,&id)==MSI_GPU_SNAPSHOT_INVALID);
    /* Repeated triangles sharing 8-bit textures and palette state. */
    msi_gpu_snapshot_reset(&cache);cache.raw_capacity=sizeof(raw);cache.decoded_capacity=sizeof(decoded)/sizeof(decoded[0]);
    unsigned hits=0,misses=0;
    for(unsigned i=0;i<20457;i++){
        unsigned j=i%16;palette(3,j+1);src[0]=(uint8_t)(3+(j%2));
        unsigned k=9999;int rc=msi_gpu_snapshot_indexed8(&cache,&s,src,64,lut,512,&k);
        CHECK(rc==MSI_GPU_SNAPSHOT_NEW||rc==MSI_GPU_SNAPSHOT_HIT);
        CHECK(value(k,0)==(j%2?0x001f:j+1));
        if(rc==MSI_GPU_SNAPSHOT_HIT)hits++;else misses++;
    }
    CHECK(misses==16&&hits==20441);
    printf("MSI_GPU_SNAPSHOT_PASS synthetic_draws=20457 hits=%u misses=%u\n",hits,misses);
    return 0;
}
