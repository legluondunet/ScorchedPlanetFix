/* SPDX-License-Identifier: GPL-3.0-or-later
 * Real native Windows x86 VEH continuation smoke, no proprietary game
 * bytes used. Executes two tiny synthetic routines with the same x86
 * callee-save/0x18-local stack shape and the original failing 3-byte
 * instructions. The existing pure policy classifier chooses recovery.
 */
typedef unsigned int DWORD;
typedef unsigned char BYTE;
typedef int LONG;
typedef unsigned long SIZE_T;
typedef void* HANDLE;
#define NULL ((void*)0)
#define EXCEPTION_ACCESS_VIOLATION 0xC0000005u
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define PAGE_EXECUTE_READWRITE 0x40u
#include "msi_smack_av_recover.h"
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,DWORD,DWORD,DWORD);
__declspec(dllimport) void* __stdcall AddVectoredExceptionHandler(unsigned long,void*);
__declspec(dllimport) int __stdcall FlushInstructionCache(HANDLE,const void*,SIZE_T);
__declspec(dllimport) HANDLE __stdcall GetCurrentProcess(void);
__declspec(dllimport) void __stdcall ExitProcess(DWORD);

typedef struct {
    DWORD ExceptionCode,ExceptionFlags;
    void* ExceptionRecord;
    void* ExceptionAddress;
    DWORD NumberParameters,ExceptionInformation[15];
} EXCEPTION_RECORD_MIN;
typedef struct {EXCEPTION_RECORD_MIN* ExceptionRecord;void* ContextRecord;} EXCEPTION_POINTERS_MIN;
typedef struct {
    DWORD ContextFlags,Dr0,Dr1,Dr2,Dr3,Dr6,Dr7;
    struct {DWORD ControlWord,StatusWord,TagWord,ErrorOffset,ErrorSelector,
        DataOffset,DataSelector; BYTE RegisterArea[80]; DWORD Cr0NpxState;} FloatSave;
    DWORD SegGs,SegFs,SegEs,SegDs,Edi,Esi,Ebx,Edx,Ecx,Eax,Ebp,Eip,SegCs,EFlags,Esp,SegSs;
    BYTE ExtendedRegisters[512];
} CONTEXT32_MIN;
typedef char smoke_requires_x86[(sizeof(void*)==4)?1:-1];
static DWORD g_bad_palette=0u,g_bad_frames=0u;
static DWORD g_epilogue_palette=0u,g_epilogue_frames=0u;
static volatile DWORD g_recovered_palette=0u,g_recovered_frames=0u;
static LONG __stdcall on_av(EXCEPTION_POINTERS_MIN* e){
    if(!e||!e->ExceptionRecord||!e->ContextRecord)return 0;
    EXCEPTION_RECORD_MIN*r=e->ExceptionRecord;
    CONTEXT32_MIN*c=(CONTEXT32_MIN*)e->ContextRecord;
    DWORD ip=c->Eip;
    DWORD mapped=ip==g_bad_palette?MSI_SMACK_ORIGINAL_AV_PALETTE:
                 ip==g_bad_frames?MSI_SMACK_ORIGINAL_AV_FRAMES:0u;
    if(!mapped||r->NumberParameters<2||r->ExceptionFlags&1u||
       (DWORD)r->ExceptionAddress!=ip)return 0;
    if(!msi_smack_is_recoverable_av(1u,1u,r->ExceptionCode,mapped,
        r->ExceptionInformation[0],r->ExceptionInformation[1],
        c->Ecx,c->Edx,0u))return 0;
    if(ip==g_bad_palette){g_recovered_palette++;c->Eip=g_epilogue_palette;}
    else {g_recovered_frames++;c->Eip=g_epilogue_frames;}
    return -1; /* Windows EXCEPTION_CONTINUE_EXECUTION */
}
static DWORD run_smoke(void){
    /* No shellcode in the actual MSI95 loader. This synthetic executable
     * buffer is only for an independent self-contained x86 regression. */
    BYTE *buf=(BYTE*)VirtualAlloc(NULL,4096u,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!buf)return 11u;
    const BYTE first[]={
        0x83,0xec,0x18,0x53,0x56,0x57,0x55,
        0xb8,0x21,0x43,0x65,0x87,0x31,0xc9,
        0x8b,0x41,0x68,
        0x5d,0x5f,0x5e,0x5b,0x83,0xc4,0x18,0xc3};
    const BYTE second[]={
        0x83,0xec,0x18,0x53,0x56,0x57,0x55,
        0xb8,0x78,0x56,0x34,0x12,0x31,0xd2,
        0x39,0x42,0x0c,
        0x5d,0x5f,0x5e,0x5b,0x83,0xc4,0x18,0xc3};
    for(unsigned i=0;i<sizeof(first);i++)buf[64u+i]=first[i];
    for(unsigned i=0;i<sizeof(second);i++)buf[128u+i]=second[i];
    g_bad_palette=(DWORD)(buf+64u+14u);
    g_bad_frames=(DWORD)(buf+128u+14u);
    g_epilogue_palette=(DWORD)(buf+64u+17u);
    g_epilogue_frames=(DWORD)(buf+128u+17u);
    if(!FlushInstructionCache(GetCurrentProcess(),buf,4096u))return 12u;
    if(!AddVectoredExceptionHandler(1,(void*)on_av))return 13u;
    DWORD (__cdecl *one)(void)=(DWORD (__cdecl*)(void))(buf+64u);
    DWORD (__cdecl *two)(void)=(DWORD (__cdecl*)(void))(buf+128u);
    if(one()!=0x87654321u||g_recovered_palette!=1u)return 21u;
    if(two()!=0x12345678u||g_recovered_frames!=1u)return 22u;
    if(one()!=0x87654321u||g_recovered_palette!=2u)return 23u;
    if(two()!=0x12345678u||g_recovered_frames!=2u)return 24u;
    return 0u;
}
void mainCRTStartup(void){ExitProcess(run_smoke());}
