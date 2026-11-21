/* Shared clean-room 32-bit x86 Smacker guard code emitter.
 * Used unchanged by the MSI95 runtime and synthetic execution smoke.
 * No original game data or proprietary instructions are distributed. */
#ifndef SCORCHED_SMACKER_GUARD_X86_H
#define SCORCHED_SMACKER_GUARD_X86_H
typedef char smack_guard_requires_32bit_dword[(sizeof(DWORD)==4)?1:-1];
enum { SMACK_GUARD_STUB_LENGTH=26, SMACK_GUARD_DETOUR_LENGTH=6 };
static void video_guard_store32(BYTE*at,DWORD value){
    for(unsigned i=0;i<4u;i++)at[i]=(BYTE)(value>>(i*8u));
}
static void video_guard_emit_rel32(BYTE*at,DWORD address,DWORD destination){
    at[0]=0xe9;
    video_guard_store32(at+1u,destination-(address+5u));
}
/* 0 mov edx,[handle_addr]; 6 test edx,edx; 8 jne +11;
 * 10 inc DWORD PTR [counter]; 16 jmp canceled-tick epilogue;
 * 21 jmp original valid handle path. */
static void video_guard_emit_trampoline(BYTE*stub,DWORD handle_addr,
        DWORD counter_addr,DWORD cancel_exit,DWORD normal_resume){
    stub[0]=0x8b;stub[1]=0x15;
    video_guard_store32(stub+2u,handle_addr);
    stub[6]=0x85;stub[7]=0xd2;
    stub[8]=0x75;stub[9]=0x0b;
    stub[10]=0xff;stub[11]=0x05;
    video_guard_store32(stub+12u,counter_addr);
    video_guard_emit_rel32(stub+16u,(DWORD)stub+16u,cancel_exit);
    video_guard_emit_rel32(stub+21u,(DWORD)stub+21u,normal_resume);
}
#endif
