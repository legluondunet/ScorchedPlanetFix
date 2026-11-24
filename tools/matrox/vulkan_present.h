/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MATROX_VULKAN_PRESENT_H
#define MATROX_VULKAN_PRESENT_H
#include <stdint.h>
/* One presenter, owning thread only. Caller owns HWND and pumps messages.
 * open: 0 success, 77 unsupported/unavailable, 1 other failure.
 * present: 0 success, 1 failure (caller must close before GDI fallback).
 * close: idempotent. No window ownership, no writes to guest pixels.
 * Fixed native 640x400 RGB565 input, pitch in bytes. */
typedef void (*MatroxVkReport)(const char *message,int code);
int matrox_vk_open(void *window,int linear,MatroxVkReport logger);
int matrox_vk_present(const uint16_t *pixels,uint32_t pitch);
void matrox_vk_close(void);
#endif
