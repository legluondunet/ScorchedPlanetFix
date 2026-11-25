/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native-resolution diagnostic session for a borrowed Vulkan device/queue.
 * All calls on the owning thread, externally synchronized with presentation.
 * No game pointers are stored; output is caller-owned scratch, never displayed.
 * The caller must close successfully (or on device loss) before destroying
 * the borrowed device. This layer neither creates nor destroys that device.
 */
#ifndef MSI_VK_SHADOW_SESSION_H
#define MSI_VK_SHADOW_SESSION_H
#include "msi_vk_batch_replay.h"
#include "msi_gpu_compare.h"
typedef struct {
    MsiVkBatchContext ctx;
    MsiVkBatchWork work;
    unsigned ready;
} MsiVkShadowSession;
static VkResult msi_vk_shadow_close(MsiVkShadowSession *s){
    s->ready=0;
    VkResult r=VK_SUCCESS;
    if(s->work.pending){
        r=msi_vk_batch_retire(&s->ctx,&s->work);
        if(s->work.pending)return r; /* ownership must survive uncertain wait */
    }
    if(s->ctx.dev){
        msi_vk_batch_pipeline_destroy(&s->ctx);
        if(s->ctx.cp)vkDestroyCommandPool(s->ctx.dev,s->ctx.cp,0);
    }
    *s=(MsiVkShadowSession){0};
    return r;
}
static VkResult msi_vk_shadow_open(MsiVkShadowSession *s,VkPhysicalDevice physical,
    VkDevice device,VkQueue queue,unsigned family,const uint32_t *vs,size_t vs_bytes,
    const uint32_t *fs,size_t fs_bytes,unsigned fault){
    if(!s||s->ctx.dev||!physical||!device||!queue)return VK_ERROR_INITIALIZATION_FAILED;
    VkImageFormatProperties p;
    VkResult r=vkGetPhysicalDeviceImageFormatProperties(physical,VK_FORMAT_R16_UINT,
        VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,0,&p);
    if(r!=VK_SUCCESS)return r;
    if(p.maxExtent.width<640u||p.maxExtent.height<400u)return VK_ERROR_FORMAT_NOT_SUPPORTED;
    r=vkGetPhysicalDeviceImageFormatProperties(physical,VK_FORMAT_R32_UINT,
        VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,0,&p);
    if(r!=VK_SUCCESS)return r;
    s->ctx.phys=physical;s->ctx.dev=device;s->ctx.queue=queue;
    vkGetPhysicalDeviceMemoryProperties(physical,&s->ctx.mem);
    VkCommandPoolCreateInfo pi={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex=family;pi.flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    r=vkCreateCommandPool(device,&pi,0,&s->ctx.cp);
    if(r==VK_SUCCESS){
        if(fault==1u)r=VK_ERROR_OUT_OF_HOST_MEMORY;
        else r=msi_vk_batch_pipeline_create(&s->ctx,vs,vs_bytes,fs,fs_bytes,fault?fault-1u:0u);
    }
    if(r!=VK_SUCCESS){msi_vk_shadow_close(s);return r;}
    s->ready=1;return VK_SUCCESS;
}
static VkResult msi_vk_shadow_compare(MsiVkShadowSession *s,const MsiGpuDrawBatch *b,
    const uint16_t *before,const uint16_t *cpu,uint16_t *scratch,MsiGpuCompare *stats,unsigned fault){
    if(!s||!s->ready||!b||!before||!cpu||!scratch||!stats||
       scratch==before||scratch==cpu||s->ctx.unusable)return VK_ERROR_INITIALIZATION_FAILED;
    /* Only RGB565 native dimensions in this initial shadow phase. */
    VkResult r=msi_vk_batch_render(&s->ctx,&s->work,b,640u,400u,before,scratch,fault);
    if(r!=VK_SUCCESS){if(s->ctx.unusable)s->ready=0;return r;}
    msi_gpu_compare(stats,b,640u,400u,before,cpu,scratch);
    return VK_SUCCESS;
}
#endif
