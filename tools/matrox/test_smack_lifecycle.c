/* SPDX-License-Identifier: GPL-3.0-or-later
 * Synthetic tests for the passive Smacker video lifetime event classifier.
 * No original game resources are needed or embedded.
 */
#include "msi_smack_lifecycle.h"
#include <stdio.h>
static unsigned ncases=0;
static int test(unsigned s0,unsigned h0,unsigned s1,unsigned h1,
                unsigned expected,const char *name){
    unsigned got=msi_smack_change_mask(s0,h0,s1,h1);
    ncases++;
    if(got!=expected){
        printf("SMACK_LIFECYCLE_FAIL %s expected=%u actual=%u\n",name,expected,got);
        return 1;
    }
    return 0;
}
static int test_emit(unsigned event,unsigned old_index,unsigned new_index,
                     unsigned emitted,unsigned expected,const char *name){
    unsigned got=(unsigned)msi_smack_should_log(event,old_index,new_index,emitted);
    ncases++;
    if(got!=expected){
        printf("SMACK_TRACE_POLICY_FAIL %s expected=%u actual=%u\n",name,expected,got);
        return 1;
    }
    return 0;
}
int main(void) {
    int fail=0;
    fail+=test(0xffffffffu,0xffffffffu,0,0,3,"first_read");
    fail+=test(0,0,1,0x1550940u,3,"successful_SmackOpen");
    fail+=test(1,0x1550940u,1,0,6,"close_leaves_state1_null");
    fail+=test(1,0,1,0,0,"no_duplicate_state1_null");
    fail+=test(2,0x1550940u,1,0,7,"change_state_and_handle_to_null");
    fail+=test(1,0x1550940u,2,0,3,"normal_close_state2");
    fail+=test(7,0x1550940u,7,0,2,"benign_status7_null_handle");
    fail+=test(5,0,6,0,1,"state_changes_without_handle");
    fail+=test(1,0,1,0x1550940u,2,"handle_reopened");
    fail+=test(1,0x1550940u,1,0x1560940u,2,"replaced_handle");
    fail+=test(1,0x1550940u,1,0x1550940u,0,"stable_playback");
    /* Owner's 14:31 trace had 702 state changes, but former 192
     * ordinary-frame report ceiling was spent at frame 199, hiding
     * important closes at frame 584 and frame 3976 and reopens. */
    fail+=test_emit(1,21,22,12,0,"routine_frame22_suppressed");
    fail+=test_emit(1,59,60,12,1,"milestone_video_frame60");
    fail+=test_emit(1,119,120,63,1,"milestone_before_budget");
    fail+=test_emit(1,179,180,64,0,"milestone_after_budget");
    fail+=test_emit(1,1,2,2,1,"first_three_video_frames");
    fail+=test_emit(1,0xffffffffu,0,64,1,"initial_unknown_counter");
    fail+=test_emit(1,285,1,192,1,"counter_reset_never_suppressed");
    fail+=test_emit(1,150,160,192,1,"counter_jump_never_suppressed");
    fail+=test_emit(2,286,286,192,1,"completed_video_close_after_old_cap");
    fail+=test_emit(3,0,1,4096,1,"new_movie_open_after_old_cap");
    fail+=test_emit(6,1,1,4096,1,"first_frame_null_never_suppressed");
    fail+=test_emit(0,1,1,0,0,"no_change_no_report");
    if(fail)return 1;
    printf("MATROX_SMACK_LIFECYCLE_PASS %u state/handle + importance-preserving trace checks\n",ncases);
    return 0;
}
