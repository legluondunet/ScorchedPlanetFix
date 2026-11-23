/* SPDX-License-Identifier: GPL-3.0-or-later
 * Passive diagnostic classification for original Matrox OEM Smacker state.
 * Original game globals are read only. Never modify video state or handle,
 * hook Win32/Smack API functions, or alter exception handling.
 */
#ifndef SCORCHED_MATROX_SMACK_LIFECYCLE_H
#define SCORCHED_MATROX_SMACK_LIFECYCLE_H
#define MSI_SMACK_EVENT_STATE_CHANGED 1u
#define MSI_SMACK_EVENT_HANDLE_CHANGED 2u
#define MSI_SMACK_EVENT_STATE1_NULL 4u
static unsigned msi_smack_change_mask(unsigned before_state,
                                      unsigned before_handle,
                                      unsigned current_state,
                                      unsigned current_handle) {
    unsigned change=0u;
    if(before_state!=current_state)change|=MSI_SMACK_EVENT_STATE_CHANGED;
    if(before_handle!=current_handle)change|=MSI_SMACK_EVENT_HANDLE_CHANGED;
    /* 1+NULL was seen in the owner's exact failing Windows minidump.
     * Merely seeing 1+NULL never establishes an impending exception:
     * it must be reported only on an observable transition. */
    if(change && current_state==1u && current_handle==0u)
        change|=MSI_SMACK_EVENT_STATE1_NULL;
    return change;
}
#endif
