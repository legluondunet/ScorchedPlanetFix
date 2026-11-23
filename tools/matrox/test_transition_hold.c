/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pure deterministic OEM video-state and scene/input guard tests.
 * No original EXE, video DLL or copyrighted data required.
 */
#include "msi_transition_hold.h"
#include <stdio.h>

static int check(int got,int expected,const char *name){
    if(got!=expected){
        printf("TRANSITION_HOLD_FAIL %s got=%d expected=%d\n",name,got,expected);
        return 1;
    }
    return 0;
}
int main(void){
    int failures=0;
    /* Actual crashed OEM Smacker state 1 with null scene ~94 ms after
     * last texture heap allocation: only this specific state can hold. */
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,12094,0,1),1,
                    "real_crash_video_state_one");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,13500,0,1),1,
                    "bounded_1500ms");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,13501,0,1),0,
                    "timeout_fail_open");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,12094,0x1550940,1),0,
                    "scene_restored");
    failures+=check(msi_transition_input_should_hold(0,1,18,12000,12094,0,1),0,
                    "unrecognized_exe");
    failures+=check(msi_transition_input_should_hold(1,0,18,12000,12094,0,1),0,
                    "not_initialized");
    failures+=check(msi_transition_input_should_hold(1,1,0,12000,12094,0,1),0,
                    "no_heap");
    failures+=check(msi_transition_input_should_hold(1,1,18,0,12094,0,1),0,
                    "no_last_allocation");
    failures+=check(msi_transition_input_should_hold(1,1,18,
                   0xfffffff0u,0x00000020u,0,1),1,"tick_wrap");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,11900,0,1),0,
                    "stale_time");
    /* Real owner successful 13:28 test had benign null-scene intervals
     * with original status 2 and status 5, previously causing 3 slow
     * 1500-ms holds. Never defer normal status transitions. */
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,12094,0,2),0,
                    "normal_menu_state_two");
    failures+=check(msi_transition_input_should_hold(1,1,81,12000,12094,0,5),0,
                    "normal_game_state_five");
    if(failures)return 1;
    puts("MATROX_TRANSITION_HOLD_PASS 12 deterministic scene/video-state/timeout tests");
    return 0;
}
