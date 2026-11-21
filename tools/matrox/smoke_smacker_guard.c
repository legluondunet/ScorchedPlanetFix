/* Synthetic native Windows x86 execution smoke.
 * Same emitter as MSI95; no original OEM executable or DLL loaded. */
typedef unsigned char BYTE;
typedef unsigned int DWORD;
#include "smacker_guard_x86.h"
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define PAGE_READWRITE 4u
#define PAGE_EXECUTE_READ 0x20u
#define NULL ((void*)0)
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,DWORD,DWORD,DWORD);
__declspec(dllimport) int __stdcall VirtualProtect(void*,DWORD,DWORD,DWORD*);
__declspec(dllimport) int __stdcall FlushInstructionCache(void*,const void*,DWORD);
__declspec(dllimport) void* __stdcall GetCurrentProcess(void);
__declspec(dllimport) void __stdcall ExitProcess(DWORD);
typedef char assert_32bit_pointer[(sizeof(void*)==4)?1:-1];
static DWORD run_smoke(void){
    BYTE*globals=(BYTE*)VirtualAlloc(NULL,4096u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    BYTE*code=(BYTE*)VirtualAlloc(NULL,4096u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!globals||!code)return 11u;
    volatile DWORD*handle=(volatile DWORD*)(void*)(globals+0u);
    volatile DWORD*skips=(volatile DWORD*)(void*)(globals+4u);
    BYTE*site=code+64u,*stub=code+128u,*cancel=code+256u;
    video_guard_emit_rel32(site,(DWORD)site,(DWORD)stub);
    site[5]=0x90;
    /* Live branch: mov eax,edx;ret. Proves the exact handle was loaded. */
    site[6]=0x89;site[7]=0xd0;site[8]=0xc3;
    /* NULL branch: return a distinct synthetic epilogue value. */
    cancel[0]=0xb8;
    video_guard_store32(cancel+1u,0x55cc0011u);
    cancel[5]=0xc3;
    video_guard_emit_trampoline(stub,(DWORD)handle,(DWORD)skips,
                                (DWORD)cancel,(DWORD)(site+6u));
    if(stub[0]!=0x8b||stub[1]!=0x15||stub[6]!=0x85||
       stub[7]!=0xd2||stub[8]!=0x75||stub[9]!=0x0b||
       stub[10]!=0xff||stub[11]!=0x05||
       stub[16]!=0xe9||stub[21]!=0xe9)return 12u;
    DWORD old=0;
    if(!VirtualProtect(code,4096u,PAGE_EXECUTE_READ,&old))return 13u;
    if(!FlushInstructionCache(GetCurrentProcess(),code,4096u))return 14u;
    DWORD (__cdecl *entry)(void)=(DWORD (__cdecl *)(void))site;
    *handle=0u;*skips=0u;
    if(entry()!=0x55cc0011u||*skips!=1u)return 21u;
    *handle=0x12345678u;
    if(entry()!=0x12345678u||*skips!=1u)return 22u;
    *handle=0xffffffffu;
    if(entry()!=0xffffffffu||*skips!=1u)return 23u;
    *handle=0u;
    if(entry()!=0x55cc0011u||*skips!=2u)return 24u;
    return 0u;
}
void mainCRTStartup(void){ExitProcess(run_smoke());}
