/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MATROX_VULKAN_PRESENT_H
#define MATROX_VULKAN_PRESENT_H
#include <stdint.h>
#include "msi_gpu_compare.h"
/* One presenter, owning thread only. Caller owns HWND and pumps messages.
 * open: 0 success, 77 unsupported/unavailable, 1 other failure.
 * present: 0 success, 1 failure (caller must close before GDI fallback),
 * 2 minimized/zero-sized window (skip without teardown),
 * 3 surface extent/out-of-date (close and recreate before next present).
 * close: idempotent. No window ownership, no writes to guest pixels.
 * Fixed native 640x400 RGB565 input, pitch in bytes. */
typedef void (*MatroxVkReport)(const char *message,int code);
int matrox_vk_open(void *window,int linear,MatroxVkReport logger);
int matrox_vk_present(const uint16_t *pixels,uint32_t pitch);
void matrox_vk_close(void);
/* Experimental native diagnostic and tagged output. Guest buffers stay read-only. */
int matrox_vk_shadow_compare(const MsiGpuDrawBatch *batch,const uint16_t *before,
    const uint16_t *cpu,uint16_t *scratch,MsiGpuCompare *stats);
int matrox_vk_shadow_tagged(const MsiGpuDrawBatch *batch,uint32_t *scratch);
int matrox_vk_shadow_tagged_roi(const MsiGpuDrawBatch *batch,uint32_t *scratch,unsigned *region_pixels);
/* Optional wait_ticks is QPC ticks spent inside the submission fence wait;
 * UINT32_MAX means unavailable. Excludes GPU command preparation. */
int matrox_vk_shadow_tagged_packed(const MsiGpuDrawBatch *batch,uint32_t *scratch,size_t capacity,MsiGpuRect *region,uint32_t *wait_ticks);
/* Owning thread, immediately after successful tagged ROI dispatch. */
unsigned matrox_vk_shadow_roi_texture_mode(void);
#endif
