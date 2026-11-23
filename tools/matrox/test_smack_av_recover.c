/* SPDX-License-Identifier: GPL-3.0-or-later
 * Synthetic classification cases against the same predicate as real VEH.
 * No original copyrighted game/DLL bytes or crash dump included.
 */
#include "msi_smack_av_recover.h"
#include <stdio.h>
static unsigned checks=0;
static int check(unsigned en,unsigned sig,unsigned code,unsigned ip,
                 unsigned op,unsigned addr,unsigned ecx,unsigned edx,
                 unsigned handle,unsigned expected,const char *name){
    unsigned actual=(unsigned)msi_smack_is_recoverable_av(
        en,sig,code,ip,op,addr,ecx,edx,handle);
    checks++;
    if(actual!=expected){
        printf("SMACK_AV_CLASSIFIER_FAIL %s expected=%u actual=%u\n",name,expected,actual);
        return 1;
    }
    return 0;
}
int main(void){
    int bad=0;
#define C(e,s,c,ip,op,addr,ecx,edx,h,x,name) \
    bad+=check(e,s,c,ip,op,addr,ecx,edx,h,x,name)
    C(1,1,0xc0000005u,0x0046d578u,0,0x68,0,1,0,1,
      "actual_original_native_windows_dump_ecx_null");
    C(1,1,0xc0000005u,0x0046d76du,0,0x0c,1,0,0,1,
      "original_wine_video_frames_edx_null");
    C(0,1,0xc0000005u,0x0046d578u,0,0x68,0,1,0,0,"default_OFF");
    C(1,0,0xc0000005u,0x0046d578u,0,0x68,0,1,0,0,"unknown_game_exe");
    C(1,1,0xc0000005u,0x0046d578u,1,0x68,0,1,0,0,"write_av_not_read");
    C(1,1,0xc0000005u,0x0046d578u,8,0x68,0,1,0,0,"execute_av_not_read");
    C(1,1,0xc0000005u,0x0046d578u,0,0x69,0,1,0,0,"wrong_palette_address");
    C(1,1,0xc0000005u,0x0046d578u,0,0x68,1,1,0,0,"non_null_ecx");
    C(1,1,0xc0000005u,0x0046d578u,0,0x68,0,1,0x1000,0,"non_null_video_handle");
    C(1,1,0x80000003u,0x0046d578u,0,0x68,0,1,0,0,"breakpoint_not_av");
    C(1,1,0xc0000005u,0x0046d76du,0,0x0c,1,1,0,0,"non_null_edx");
    C(1,1,0xc0000005u,0x0046d76du,0,0x0d,1,0,0,0,"wrong_frames_address");
    C(1,1,0xc0000005u,0x0046d76du,0,0x0c,1,0,0x1000,0,"live_handle");
    C(1,1,0xc0000005u,0x0046d761u,0,0x0c,1,0,0,0,"earlier_instruction");
    C(1,1,0xc0000005u,0x0046d579u,0,0x68,0,1,0,0,"off_by_one_fault_ip");
    C(1,1,0xc0000005u,0x0048302du,0,0x68,0,0,0,0,"other_scene_list_av");
    C(1,1,0xc0000005u,0x0046d578u,0,0x0c,0,1,0,0,"palette_ip_wrong_offset");
    C(1,1,0xc0000005u,0x0046d76du,0,0x68,1,0,0,0,"frames_ip_wrong_offset");
    if(bad)return 1;
    printf("MATROX_SMACK_AV_RECOVERY_POLICY_PASS %u synthetic strict matches\n",checks);
    return 0;
}
