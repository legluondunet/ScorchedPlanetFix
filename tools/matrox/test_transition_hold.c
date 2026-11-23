/* SPDX-License-Identifier: GPL-3.0-or-later
 * Deterministic source-level regression for original OEM input queue hold.
 * No copyrighted game bytes or executable needed.
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
    /* The observed actual crash was ~94ms after scene heap allocation. */
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,12094,0),1,
                    "real_enter_scene_gap");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,13500,0),1,
                    "bounded_1500ms");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,13501,0),0,
                    "timeout_fail_open");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,12094,0x1550940),0,
                    "scene_restored_releases_queued_keys");
    failures+=check(msi_transition_input_should_hold(0,1,18,12000,12094,0),0,
                    "unrecognized_exe_no_guard");
    failures+=check(msi_transition_input_should_hold(1,0,18,12000,12094,0),0,
                    "not_initialized");
    failures+=check(msi_transition_input_should_hold(1,1,0,12000,12094,0),0,
                    "no_heap_yet");
    failures+=check(msi_transition_input_should_hold(1,1,18,0,12094,0),0,
                    "no_recent_allocation");
    failures+=check(msi_transition_input_should_hold(1,1,18,
                   0xfffffff0u,0x00000020u,0),1,"tick_wrap");
    failures+=check(msi_transition_input_should_hold(1,1,18,12000,11900,0),0,
                    "stale_time");
    if(failures)return 1;
    puts("MATROX_TRANSITION_HOLD_PASS 10 deterministic scene/null/timeout tests");
    return 0;
}
