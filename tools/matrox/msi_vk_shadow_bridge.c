/* SPDX-License-Identifier: GPL-3.0-or-later */
#define VK_NO_PROTOTYPES
#include "msi_vk_shadow_bridge.h"
/* Explicit borrowed dispatch: no Vulkan import library, global loader
 * reference, or accidental device selection distinct from the presenter. */
#define INSTANCE_COMMANDS(X) \
    X(vkGetPhysicalDeviceImageFormatProperties) \
    X(vkGetPhysicalDeviceMemoryProperties)
#define DEVICE_COMMANDS(X) \
    X(vkAllocateCommandBuffers) \
    X(vkAllocateDescriptorSets) \
    X(vkAllocateMemory) \
    X(vkBeginCommandBuffer) \
    X(vkBindBufferMemory) \
    X(vkBindImageMemory) \
    X(vkCmdBeginRenderPass) \
    X(vkCmdBindDescriptorSets) \
    X(vkCmdBindPipeline) \
    X(vkCmdBindVertexBuffers) \
    X(vkCmdCopyBufferToImage) \
    X(vkCmdCopyImageToBuffer) \
    X(vkCmdDraw) \
    X(vkCmdEndRenderPass) \
    X(vkCmdPipelineBarrier) \
    X(vkCmdPushConstants) \
    X(vkCmdSetScissor) \
    X(vkCmdSetViewport) \
    X(vkCreateBuffer) \
    X(vkCreateCommandPool) \
    X(vkCreateDescriptorPool) \
    X(vkCreateDescriptorSetLayout) \
    X(vkCreateFramebuffer) \
    X(vkCreateGraphicsPipelines) \
    X(vkCreateImage) \
    X(vkCreateImageView) \
    X(vkCreatePipelineLayout) \
    X(vkCreateRenderPass) \
    X(vkCreateSampler) \
    X(vkCreateShaderModule) \
    X(vkDestroyBuffer) \
    X(vkDestroyCommandPool) \
    X(vkDestroyDescriptorPool) \
    X(vkDestroyDescriptorSetLayout) \
    X(vkDestroyFramebuffer) \
    X(vkDestroyImage) \
    X(vkDestroyImageView) \
    X(vkDestroyPipeline) \
    X(vkDestroyPipelineLayout) \
    X(vkDestroyRenderPass) \
    X(vkDestroySampler) \
    X(vkDestroyShaderModule) \
    X(vkEndCommandBuffer) \
    X(vkFreeCommandBuffers) \
    X(vkFreeMemory) \
    X(vkGetBufferMemoryRequirements) \
    X(vkGetImageMemoryRequirements) \
    X(vkMapMemory) \
    X(vkQueueSubmit) \
    X(vkQueueWaitIdle) \
    X(vkResetCommandBuffer) \
    X(vkUnmapMemory) \
    X(vkUpdateDescriptorSets)

#define DECLARE(name) static PFN_##name name;
INSTANCE_COMMANDS(DECLARE)
DEVICE_COMMANDS(DECLARE)
#undef DECLARE
#include "msi_vk_shadow_session.h"
static MsiVkShadowSession shadow;
VkResult msi_vk_bridge_open(VkInstance instance,VkPhysicalDevice physical,VkDevice device,
    VkQueue queue,unsigned family,PFN_vkGetInstanceProcAddr gipa,
    const uint32_t *vs,size_t vs_bytes,const uint32_t *fs,size_t fs_bytes){
    if(shadow.ctx.dev||!instance||!physical||!device||!queue||!gipa)
        return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetDeviceProcAddr gdpa=(PFN_vkGetDeviceProcAddr)gipa(instance,"vkGetDeviceProcAddr");
    if(!gdpa)return VK_ERROR_INITIALIZATION_FAILED;
#define LOAD_I(name) name=(PFN_##name)gipa(instance,#name);if(!name)return VK_ERROR_INITIALIZATION_FAILED;
    INSTANCE_COMMANDS(LOAD_I)
#undef LOAD_I
#define LOAD_D(name) name=(PFN_##name)gdpa(device,#name);if(!name)return VK_ERROR_INITIALIZATION_FAILED;
    DEVICE_COMMANDS(LOAD_D)
#undef LOAD_D
    return msi_vk_shadow_open(&shadow,physical,device,queue,family,vs,vs_bytes,fs,fs_bytes,0);
}
VkResult msi_vk_bridge_compare(const MsiGpuDrawBatch *batch,const uint16_t *before,
    const uint16_t *cpu,uint16_t *scratch,MsiGpuCompare *stats){
    return msi_vk_shadow_compare(&shadow,batch,before,cpu,scratch,stats,0);
}
VkResult msi_vk_bridge_tagged(const MsiGpuDrawBatch *batch,uint32_t *scratch,
    const uint32_t *vs,size_t vs_bytes,const uint32_t *fs,size_t fs_bytes){
    return msi_vk_shadow_tagged(&shadow,batch,scratch,vs,vs_bytes,fs,fs_bytes,0);
}
VkResult msi_vk_bridge_tagged_roi(const MsiGpuDrawBatch *batch,uint32_t *scratch,
    const uint32_t *vs,size_t vs_bytes,const uint32_t *fs,size_t fs_bytes,unsigned *region_pixels){
    shadow.tagged.texture_cache.last_mode=3u;
    if(!region_pixels)return VK_ERROR_INITIALIZATION_FAILED;
    return msi_vk_shadow_tagged_impl(&shadow,batch,scratch,vs,vs_bytes,fs,fs_bytes,0,region_pixels,0,NULL);
}
VkResult msi_vk_bridge_tagged_packed(const MsiGpuDrawBatch *batch,uint32_t *scratch,size_t capacity,
    MsiGpuRect *region,const uint32_t *vs,size_t vs_bytes,const uint32_t *fs,size_t fs_bytes){
    shadow.tagged.texture_cache.last_mode=3u;
    if(!region)return VK_ERROR_INITIALIZATION_FAILED;
    return msi_vk_shadow_tagged_impl(&shadow,batch,scratch,vs,vs_bytes,fs,fs_bytes,0,NULL,capacity,region);
}
unsigned msi_vk_bridge_roi_texture_mode(void){
    if(!shadow.ready||!shadow.tagged.pipe||shadow.tagged.unusable)return 3u;
    return shadow.tagged.texture_cache.last_mode;
}
VkResult msi_vk_bridge_close(void){return msi_vk_shadow_close(&shadow);}
#undef INSTANCE_COMMANDS
#undef DEVICE_COMMANDS
