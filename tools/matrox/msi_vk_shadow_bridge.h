/* SPDX-License-Identifier: GPL-3.0-or-later
 * One optional native shadow session on a borrowed presenter device.
 * Owning thread only. No dependency on Win32, no Vulkan loader ownership.
 * Close before the presenter destroys its device. An ambiguous wait error
 * requires retrying close; never destroy a device with outstanding work.
 */
#ifndef MSI_VK_SHADOW_BRIDGE_H
#define MSI_VK_SHADOW_BRIDGE_H
#include <vulkan/vulkan.h>
#include "msi_gpu_compare.h"
VkResult msi_vk_bridge_open(VkInstance instance,VkPhysicalDevice physical,VkDevice device,
    VkQueue queue,unsigned family,PFN_vkGetInstanceProcAddr gipa,
    const uint32_t *vs,size_t vs_bytes,const uint32_t *fs,size_t fs_bytes);
VkResult msi_vk_bridge_compare(const MsiGpuDrawBatch *batch,const uint16_t *before,
    const uint16_t *cpu,uint16_t *scratch,MsiGpuCompare *stats);
VkResult msi_vk_bridge_tagged(const MsiGpuDrawBatch *batch,uint32_t *scratch,
    const uint32_t *vs,size_t vs_bytes,const uint32_t *fs,size_t fs_bytes);
VkResult msi_vk_bridge_tagged_roi(const MsiGpuDrawBatch *batch,uint32_t *scratch,
    const uint32_t *vs,size_t vs_bytes,const uint32_t *fs,size_t fs_bytes,unsigned *region_pixels);
VkResult msi_vk_bridge_close(void);
#endif
