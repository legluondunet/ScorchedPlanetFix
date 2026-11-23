/* SPDX-License-Identifier: GPL-3.0-or-later
 * Narrow OEM scene-transition input-queue guard. No game memory writes.
 * Only original video state=1 (confirmed at the real AV) can hold.\n * Input remains queued, not discarded. All arithmetic is unsigned to
 * tolerate GetTickCount wraparound. The caller must verify the original
 * splanetw.exe instruction bytes before applying the predicate.
 */
#ifndef SCORCHED_MSI_TRANSITION_HOLD_H
#define SCORCHED_MSI_TRANSITION_HOLD_H
#define MSI_TRANSITION_INPUT_HOLD_MAX_MS 1500u
static int msi_transition_input_should_hold(unsigned signature_valid,
                                             unsigned initialized,
                                             unsigned live_heaps,
                                             unsigned last_heap_alloc_tick,
                                             unsigned now_tick,
                                             unsigned scene_object_ptr,
                                             unsigned video_state) {
    if(!signature_valid || !initialized || !live_heaps ||
       !last_heap_alloc_tick || scene_object_ptr!=0u ||
       video_state!=1u) return 0;
    return (unsigned)(now_tick-last_heap_alloc_tick)<=
           MSI_TRANSITION_INPUT_HOLD_MAX_MS;
}
#endif
