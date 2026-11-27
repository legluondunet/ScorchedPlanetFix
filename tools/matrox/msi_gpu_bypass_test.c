/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "msi_gpu_bypass.h"
static void test_pilot_oracle(void){
    static uint16_t cpu[640u*400u];
    static uint8_t mask[640u*400u];
    MsiGpuDrawBatch b={0};b.count=1;b.draws[0].flags=73;
    b.draws[0].bounds=(MsiGpuRect){10,10,20,20};
    b.vertices[0].xy[0]=10.f;b.vertices[0].xy[1]=10.f;
    b.vertices[1].xy[0]=20.f;b.vertices[1].xy[1]=10.f;
    b.vertices[2].xy[0]=10.f;b.vertices[2].xy[1]=20.f;
    for(unsigned i=0;i<3;i++)b.vertices[i].iw=1.f;
    MsiGpuRect roi;assert(msi_gpu_native_tagged_region(&b,&roi));
    assert(roi.x0==9&&roi.y0==9&&roi.x1==21&&roi.y1==21);
    uint32_t tags[144];for(unsigned i=0;i<144;i++)tags[i]=65536u;
    size_t a=11u*640u+11u,z=12u*640u+12u,edge=15u*640u+14u;
    mask[a]=mask[z]=mask[edge]=2;cpu[a]=0;cpu[z]=0xffff;cpu[edge]=0x1234;
    tags[2u*12u+2u]=0;tags[3u*12u+3u]=0xfffe;
    MsiGpuPilotOracle r;
    assert(msi_gpu_pilot_oracle(&r,&b,cpu,mask,tags,144,roi));
    assert(r.cpu_written==3&&r.gpu_written==2&&r.exact==1&&r.one==1&&r.edge_coverage==1);
    tags[3u*12u+3u]=0;
    assert(!msi_gpu_pilot_oracle(&r,&b,cpu,mask,tags,144,roi)&&r.bad==1);
    tags[3u*12u+3u]=0xfffe;tags[0]=1;
    assert(!msi_gpu_pilot_oracle(&r,&b,cpu,mask,tags,144,roi)&&r.outside==1);
    tags[0]=65537u;
    assert(!msi_gpu_pilot_oracle(&r,&b,cpu,mask,tags,144,roi)&&r.invalid==1);
    tags[0]=65536u;mask[a]=1;
    assert(!msi_gpu_pilot_oracle(&r,&b,cpu,mask,tags,144,roi)&&r.invalid==1);
    mask[a]=2;assert(!msi_gpu_pilot_oracle(&r,&b,cpu,mask,tags,143,roi));
    tags[2u*12u+2u]=65536u;
    assert(!msi_gpu_pilot_oracle(&r,&b,cpu,mask,tags,144,roi)&&r.bad==1);
    puts("MSI_GPU_PILOT_ORACLE_PASS color=1 edge=1 outside=1 invalid=1");
}
static void test_packed_union(void){
    enum {N=640*400};
    static uint16_t before[N],live[N],reference[N];
    static uint8_t under[N],ref_under[N];
    static uint32_t full[N],packed[N];
    MsiGpuDrawBatch b={0};MsiGpuBypass a,z;
    for(unsigned trial=0;trial<8;trial++){
        b.count=trial?32u:2u;
        for(unsigned k=0;k<b.count;k++){
            unsigned x=(k*137u+trial*19u)%600u,y=(k*71u+trial*29u)%360u;
            if(!trial){x=k?450u:30u;y=30u;}
            MsiGpuRect q={x,y,x+1u+(k*7u+trial)%40u,y+1u+(k*3u+trial)%40u};
            b.draws[k].bounds=q;b.draws[k].flags=73;
            MsiGpuVertex *v=&b.vertices[3u*k];
            v[0].xy[0]=(float)q.x0;v[0].xy[1]=(float)q.y0;
            v[1].xy[0]=(float)q.x1;v[1].xy[1]=(float)q.y0;
            v[2].xy[0]=(float)q.x0;v[2].xy[1]=(float)q.y1;
            for(unsigned j=0;j<3;j++)v[j].iw=1.f;
        }
        MsiGpuRect roi;assert(msi_gpu_native_tagged_region(&b,&roi));
        unsigned rw=roi.x1-roi.x0,rh=roi.y1-roi.y0;
        for(unsigned j=0;j<N;j++){before[j]=live[j]=reference[j]=(uint16_t)j;under[j]=ref_under[j]=3;full[j]=65536u;}
        for(unsigned k=0;k<b.count;k++){
            MsiGpuRect q=b.draws[k].bounds;
            for(unsigned y=q.y0;y<q.y1;y++)for(unsigned x=q.x0;x<q.x1;x++){
                unsigned j=y*640u+x;full[j]=j&1u?before[j]:0x5555u;
            }
        }
        for(unsigned y=roi.y0;y<roi.y1;y++)for(unsigned x=roi.x0;x<roi.x1;x++)
            packed[(y-roi.y0)*rw+x-roi.x0]=full[y*640u+x];
        assert(msi_gpu_bypass_tagged_batch_commit(&a,640,400,&b,reference,ref_under,before,full));
        assert(msi_gpu_packed_batch_commit(&z,&b,live,under,before,packed,(size_t)rw*rh,roi));
        assert(a.written==z.written&&a.changed==z.changed);
        for(unsigned j=0;j<N;j++){assert(live[j]==reference[j]&&under[j]==ref_under[j]);live[j]=before[j];under[j]=3;}
        /* Guard ring and an interior hole in the two-rectangle fixture. */
        size_t bad=trial?0u:(size_t)(30u-roi.y0)*rw+200u-roi.x0;
        assert(packed[bad]==65536u);packed[bad]=42u;
        assert(!msi_gpu_packed_batch_commit(&z,&b,live,under,before,packed,(size_t)rw*rh,roi));
        for(unsigned j=0;j<N;j++)assert(live[j]==before[j]&&under[j]==3);
    }
    puts("MSI_GPU_PACKED_UNION_PASS cases=8 exact=2048000 holes=8");
}
int main(void){
    test_packed_union();
    test_pilot_oracle();
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
    b.count=1;b.draws[0].flags=73;
    uint32_t tagged[16];
    for(unsigned i=0;i<16;i++){live[i]=before[i];under[i]=3;tagged[i]=65536u;}
    tagged[0]=0x1234;tagged[1]=0;tagged[2]=65535u;
    live[15]=7;
    assert(!msi_gpu_bypass_tagged_commit(&r,4,4,&b,live,under,before,tagged)&&under[0]==3);
    live[15]=before[15];tagged[15]=0;
    assert(!msi_gpu_bypass_tagged_commit(&r,4,4,&b,live,under,before,tagged)&&under[0]==3);
    tagged[15]=65536u;tagged[3]=65537u;
    assert(!msi_gpu_bypass_tagged_commit(&r,4,4,&b,live,under,before,tagged)&&under[0]==3);
    tagged[3]=65536u;
    assert(msi_gpu_bypass_tagged_commit(&r,4,4,&b,live,under,before,tagged));
    assert(r.written==3&&r.changed==2&&live[1]==0&&live[2]==65535u&&under[0]==2&&under[3]==3);
    /* Two disjoint bounds: the gap lies inside the enclosing AABB,
     * but is not in either rectangle. Reject it transactionally. */
    b.count=2;b.draws[0].bounds=(MsiGpuRect){0,0,1,2};
    b.draws[1].bounds=(MsiGpuRect){3,0,4,2};b.draws[1].flags=73;
    for(unsigned i=0;i<16;i++){live[i]=before[i];under[i]=3;tagged[i]=65536u;}
    tagged[0]=before[0];tagged[3]=0;tagged[1]=42;
    assert(!msi_gpu_bypass_tagged_batch_commit(&r,4,4,&b,live,under,before,tagged)&&under[0]==3);
    tagged[1]=65536u;b.draws[1].flags=9;
    assert(!msi_gpu_bypass_tagged_batch_commit(&r,4,4,&b,live,under,before,tagged)&&under[0]==3);
    b.draws[1].flags=73;live[15]=7;
    assert(!msi_gpu_bypass_tagged_batch_commit(&r,4,4,&b,live,under,before,tagged)&&under[0]==3);
    live[15]=before[15];
    assert(msi_gpu_bypass_tagged_batch_commit(&r,4,4,&b,live,under,before,tagged));
    assert(r.written==2&&r.changed==1&&live[3]==0&&under[0]==2&&under[3]==2&&under[1]==3);
    puts("MSI_GPU_BYPASS_BATCH_PASS");
    puts("MSI_GPU_BYPASS_TAGGED_PASS");
    puts("MSI_GPU_BYPASS_PASS");return 0;
}
