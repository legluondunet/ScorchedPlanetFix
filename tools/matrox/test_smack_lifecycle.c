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
    if(fail)return 1;
    printf("MATROX_SMACK_LIFECYCLE_PASS %u read-only transition classifications\n",ncases);
    return 0;
}
