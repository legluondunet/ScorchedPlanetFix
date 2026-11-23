/* SPDX-License-Identifier: GPL-3.0-or-later
 * Exact original Matrox OEM Smacker NULL-dereference recovery predicate.
 * No game-EXE/Smack DLL byte modifications. Called from the existing VEH
 * only after exact original OEM byte signatures and callback stack check.
 */
#ifndef SCORCHED_MATROX_SMACK_AV_RECOVER_H
#define SCORCHED_MATROX_SMACK_AV_RECOVER_H
#define MSI_SMACK_ORIGINAL_AV_PALETTE 0x0046D578u
#define MSI_SMACK_ORIGINAL_AV_FRAMES  0x0046D76Du
#define MSI_SMACK_ORIGINAL_EPILOGUE   0x0046D7D2u
#define MSI_SMACK_AV_MAX_RECOVERIES 16u
static int msi_smack_is_recoverable_av(unsigned enabled,
                                      unsigned signed_oem,
                                      unsigned exception_code,
                                      unsigned fault_ip,
                                      unsigned operation,
                                      unsigned read_address,
                                      unsigned ecx,
                                      unsigned edx,
                                      unsigned live_smack_handle) {
    if(!enabled || !signed_oem || exception_code!=0xC0000005u ||
       operation!=0u || live_smack_handle!=0u)return 0;
    if(fault_ip==MSI_SMACK_ORIGINAL_AV_PALETTE)
        return ecx==0u && read_address==0x68u;
    if(fault_ip==MSI_SMACK_ORIGINAL_AV_FRAMES)
        return edx==0u && read_address==0x0Cu;
    return 0;
}
#endif
