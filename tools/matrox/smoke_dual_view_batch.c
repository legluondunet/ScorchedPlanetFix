/* SPDX-License-Identifier: GPL-3.0-or-later
 * Standalone native x86 proof: protect ONLY the public alias of a shared
 * section. Replay a real immutable indexed8 batch through the private alias
 * before making the public alias accessible. No game, Vulkan, or runtime
 * changes. No allocator, diagnostics, or graphics API in the VEH.
 * This deliberately proves same-thread direct access, not arbitrary kernel
 * copies, asynchronous guest access, or concurrent MSI calls.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "msi_gpu_deferred_finish.h"
#define W 640u
#define H 400u
#define N (W*H)
#define BYTES (N*2u)
#define NEED(e) do{if(!(e)){fprintf(stderr,"dual-view proof failed at %d: %s\n",__LINE__,#e);exit(1);}}while(0)
static uint16_t *guest,*engine;
static uint16_t before[N],cpu[N],expected[N];
static uint8_t under[N],mask[N],expected_mask[N],raw[4096];
static uint32_t decoded[64];
static MsiGpuDrawBatch batch;
static DWORD owner;
static volatile LONG armed,reads,writes,explicit_flushes,retirements;
static unsigned target;
static int finish_pending(void){
    MsiGpuBypass result;
    /* Guest view must stay inaccessible during ALL color/ownership writes. */
    int rc=msi_gpu_deferred_finish(&result,&batch,W,H,engine,under,before,0,NULL,cpu,mask);
    if(rc!=MSI_DEFER_FINISH_CPU||!result.written)return 0;
    if(engine[target]!=expected[target]||under[target]!=expected_mask[target])return 0;
    MEMORY_BASIC_INFORMATION mbi;
    if(VirtualQuery(guest,&mbi,sizeof(mbi))!=sizeof(mbi)||mbi.Protect!=PAGE_NOACCESS)return 0;
    DWORD old=0;
    if(!VirtualProtect(guest,BYTES,PAGE_READWRITE,&old)||old!=PAGE_NOACCESS)return 0;
    InterlockedIncrement(&retirements);
    InterlockedExchange(&armed,0);
    return 1;
}
static LONG CALLBACK fault(EXCEPTION_POINTERS *p){
    if(!p||!p->ExceptionRecord)return EXCEPTION_CONTINUE_SEARCH;
    EXCEPTION_RECORD *r=p->ExceptionRecord;
    if(r->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION||r->NumberParameters<2||
       (r->ExceptionInformation[0]!=0&&r->ExceptionInformation[0]!=1)||
       (uintptr_t)r->ExceptionInformation[1]-(uintptr_t)guest>=BYTES||
       GetCurrentThreadId()!=owner||InterlockedCompareExchange(&armed,2,1)!=1)
        return EXCEPTION_CONTINUE_SEARCH;
    if(!finish_pending())return EXCEPTION_CONTINUE_SEARCH;
    if(r->ExceptionInformation[0]==0)InterlockedIncrement(&reads);
    else InterlockedIncrement(&writes);
    return EXCEPTION_CONTINUE_EXECUTION;
}
static void prepare(void){
    NEED(!armed);
    for(unsigned i=0;i<N;i++){before[i]=(uint16_t)(0x1234u+i%17u);engine[i]=before[i];under[i]=3;}
    msi_gpu_draw_batch_reset(&batch);
    batch.snapshots.raw=raw;batch.snapshots.raw_capacity=sizeof(raw);
    batch.snapshots.decoded=decoded;batch.snapshots.decoded_capacity=sizeof(decoded)/sizeof(decoded[0]);
    uint8_t src[4]={1,1,1,1},lut[512]={0};
    lut[2]=0x1f;lut[3]=0xf8; /* magenta */
    MsiGpuTexture state={2,2,8,1,0,65535u,0,0,0};
    MsiGpuVertex v[3]={{{20,30},{.1f,.1f},1,{255,255,255}},
        {{110,31},{.9f,.1f},.8f,{255,255,255}},
        {{21,111},{.1f,.9f},1.2f,{255,255,255}}};
    NEED(msi_gpu_draw_batch_push(&batch,v,(MsiGpuRect){20,30,111,112},73,&state,src,4,lut,512)==MSI_GPU_DRAW_ACCEPT);
    /* Palette change must not mutate first draw. Second draw overlaps it. */
    lut[2]=0xe0;lut[3]=0x07;
    for(unsigned j=0;j<3;j++)v[j].xy[0]+=10.f;
    NEED(msi_gpu_draw_batch_push(&batch,v,(MsiGpuRect){30,30,121,112},73,&state,src,4,lut,512)==MSI_GPU_DRAW_ACCEPT);
    NEED(batch.snapshots.count==2);
    /* The caller's palette is now gone; only immutable snapshots survive. */
    NEED(msi_gpu_cpu_replay(&batch,W,H,before,expected,expected_mask));
    target=40u*W+40u;
    NEED(expected[target]==0x07e0&&expected_mask[target]==2&&before[target]!=expected[target]);
    DWORD old=0;
    InterlockedExchange(&armed,1);
    NEED(VirtualProtect(guest,BYTES,PAGE_NOACCESS,&old)&&old==PAGE_READWRITE);
    /* Private view remains writable/readable under public protection. */
    NEED(engine[target]==before[target]);
}
static void check_all(unsigned overwritten){
    NEED(!armed);
    for(unsigned i=0;i<N;i++){
        unsigned want=i==overwritten?0xdead:expected[i];
        NEED(guest[i]==want&&engine[i]==want);
        NEED(under[i]==(expected_mask[i]?expected_mask[i]:3));
    }
}
int main(void){
    HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,BYTES,NULL);
    NEED(section);
    guest=(uint16_t*)MapViewOfFile(section,FILE_MAP_READ|FILE_MAP_WRITE,0,0,BYTES);
    engine=(uint16_t*)MapViewOfFile(section,FILE_MAP_READ|FILE_MAP_WRITE,0,0,BYTES);
    NEED(guest&&engine&&guest!=engine);
    CloseHandle(section);
    owner=GetCurrentThreadId();
    void *veh=AddVectoredExceptionHandler(1,fault);NEED(veh);
    /* Read retained alias must see finished batch, not original framebuffer. */
    prepare();
    NEED(*(volatile uint16_t*)(guest+target)==expected[target]);
    NEED(reads==1&&writes==0&&retirements==1);
    check_all(N);
    /* Write retained alias happens AFTER CPU retirement. */
    prepare();
    *(volatile uint16_t*)(guest+target)=0xdead;
    NEED(reads==1&&writes==1&&retirements==2);
    check_all(target);
    /* Normal boundary retires while the public alias is still protected. */
    prepare();
    NEED(InterlockedCompareExchange(&armed,2,1)==1&&finish_pending());
    InterlockedIncrement(&explicit_flushes);
    NEED(retirements==3&&explicit_flushes==1);
    check_all(N);
    /* Both views share subsequent ordinary writes without copying. */
    guest[N-1]=0x4242;NEED(engine[N-1]==0x4242);
    NEED(RemoveVectoredExceptionHandler(veh));
    NEED(UnmapViewOfFile(guest)&&UnmapViewOfFile(engine));
    puts("MSI95_DUAL_VIEW_BATCH_PASS read=1 write=1 explicit=1 draws=2");
    return 0;
}
