/* SPDX-License-Identifier: GPL-3.0-or-later
 * Recoverable, synchronous opaque batch replay. Includes no loader, allocation
 * of host memory, logging, or process termination. Caller owns a ready Vulkan
 * device/pipeline and a separate output buffer. On a failed queue wait the
 * work stays owned and pending unless the device is lost; caller MUST stop
 * using the context and retire pending work before destroying owned objects.
 * Never publish output on failure.
 */
#ifndef MSI_VK_BATCH_REPLAY_H
#define MSI_VK_BATCH_REPLAY_H
#include <vulkan/vulkan.h>
#include <stddef.h>
#include "msi_gpu_draw_batch.h"
#define MVK_S(t) {.sType=VK_STRUCTURE_TYPE_##t}
typedef struct {
    VkPhysicalDevice phys;VkDevice dev;VkQueue queue;VkCommandPool cp;
    VkPhysicalDeviceMemoryProperties mem;
    VkDescriptorSetLayout dsl;VkPipelineLayout pl;VkRenderPass rp,rp_load;
    VkPipeline pipe;VkSampler sampler;VkImageView dummy;
    unsigned unusable; /* never reset except by a fresh device context */
} MsiVkBatchContext;
typedef struct {VkBuffer b;VkDeviceMemory m;VkDeviceSize size;} MsiVkBuffer;
typedef struct {VkImage i;VkDeviceMemory m;VkImageView v;} MsiVkImage;
typedef struct {
    MsiVkImage textures[MSI_GPU_SNAPSHOT_MAX],out;
    MsiVkBuffer stage,vertices,read,initial;
    VkDescriptorPool pool;VkFramebuffer fb;VkCommandBuffer cmd;
    unsigned pending;
} MsiVkBatchWork;
typedef struct {float logical[2],target[2];uint32_t flags,reserved;} MsiVkPush;
/* Persistent pipeline objects are owned by this context; device, queue,
 * command pool and dummy image are borrowed. Destroy only after all work
 * has drained. No device teardown, host allocation or fatal exit here. */
static void msi_vk_batch_pipeline_destroy(MsiVkBatchContext *c){
    if(c->pipe)vkDestroyPipeline(c->dev,c->pipe,0);
    if(c->rp_load)vkDestroyRenderPass(c->dev,c->rp_load,0);
    if(c->rp)vkDestroyRenderPass(c->dev,c->rp,0);
    if(c->pl)vkDestroyPipelineLayout(c->dev,c->pl,0);
    if(c->dsl)vkDestroyDescriptorSetLayout(c->dev,c->dsl,0);
    if(c->sampler)vkDestroySampler(c->dev,c->sampler,0);
    c->pipe=VK_NULL_HANDLE;c->rp_load=c->rp=VK_NULL_HANDLE;
    c->pl=VK_NULL_HANDLE;c->dsl=VK_NULL_HANDLE;c->sampler=VK_NULL_HANDLE;
}
static VkResult msi_vk_batch_pipeline_create(MsiVkBatchContext *c,
    const uint32_t *vs_code,size_t vs_bytes,const uint32_t *fs_code,size_t fs_bytes,unsigned fault){
    if(!c||!c->dev||c->unusable||!vs_code||!fs_code||!vs_bytes||!fs_bytes||vs_bytes%4u||fs_bytes%4u||
       c->pipe||c->rp||c->rp_load||c->pl||c->dsl||c->sampler)return VK_ERROR_INITIALIZATION_FAILED;
    VkResult rc=VK_SUCCESS;VkShaderModule vs=VK_NULL_HANDLE,fs=VK_NULL_HANDLE;
#define MP_TRY(x) do{rc=(x);if(rc!=VK_SUCCESS)goto done;}while(0)
#define MP_FAULT(n) do{if(fault==(n)){rc=VK_ERROR_OUT_OF_HOST_MEMORY;goto done;}}while(0)
    VkSamplerCreateInfo si=MVK_S(SAMPLER_CREATE_INFO);si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.maxLod=0;
    MP_TRY(vkCreateSampler(c->dev,&si,0,&c->sampler));MP_FAULT(1);
    VkDescriptorSetLayoutBinding bindings[2]={
        {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,0},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,0}};
    VkDescriptorSetLayoutCreateInfo dl=MVK_S(DESCRIPTOR_SET_LAYOUT_CREATE_INFO);dl.bindingCount=2;dl.pBindings=bindings;MP_TRY(vkCreateDescriptorSetLayout(c->dev,&dl,0,&c->dsl));MP_FAULT(2);
    VkPushConstantRange range={VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(MsiVkPush)};
    VkPipelineLayoutCreateInfo li=MVK_S(PIPELINE_LAYOUT_CREATE_INFO);li.setLayoutCount=1;li.pSetLayouts=&c->dsl;li.pushConstantRangeCount=1;li.pPushConstantRanges=&range;
    MP_TRY(vkCreatePipelineLayout(c->dev,&li,0,&c->pl));MP_FAULT(3);
    VkAttachmentDescription ad={0};ad.format=VK_FORMAT_R16_UINT;ad.samples=VK_SAMPLE_COUNT_1_BIT;ad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;ad.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    ad.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;ad.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;ad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;ad.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ar={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub={0};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ar;
    VkRenderPassCreateInfo ri=MVK_S(RENDER_PASS_CREATE_INFO);ri.attachmentCount=1;ri.pAttachments=&ad;ri.subpassCount=1;ri.pSubpasses=&sub;MP_TRY(vkCreateRenderPass(c->dev,&ri,0,&c->rp));MP_FAULT(4);
    ad.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;ad.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    MP_TRY(vkCreateRenderPass(c->dev,&ri,0,&c->rp_load));MP_FAULT(5);
    VkShaderModuleCreateInfo sm=MVK_S(SHADER_MODULE_CREATE_INFO);sm.codeSize=vs_bytes;sm.pCode=vs_code;
    MP_TRY(vkCreateShaderModule(c->dev,&sm,0,&vs));MP_FAULT(6);
    sm.codeSize=fs_bytes;sm.pCode=fs_code;MP_TRY(vkCreateShaderModule(c->dev,&sm,0,&fs));MP_FAULT(7);
    VkPipelineShaderStageCreateInfo stages[2]={MVK_S(PIPELINE_SHADER_STAGE_CREATE_INFO),MVK_S(PIPELINE_SHADER_STAGE_CREATE_INFO)};
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[0].pName="main";stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;stages[1].pName="main";
    VkVertexInputBindingDescription bd={0,sizeof(MsiGpuVertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[]={{0,0,VK_FORMAT_R32G32_SFLOAT,offsetof(MsiGpuVertex,xy)},{1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(MsiGpuVertex,uv)},{2,0,VK_FORMAT_R32_SFLOAT,offsetof(MsiGpuVertex,iw)},{3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(MsiGpuVertex,m)}};
    VkPipelineVertexInputStateCreateInfo vi=MVK_S(PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&bd;vi.vertexAttributeDescriptionCount=4;vi.pVertexAttributeDescriptions=attrs;
    VkPipelineInputAssemblyStateCreateInfo ia=MVK_S(PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp=MVK_S(PIPELINE_VIEWPORT_STATE_CREATE_INFO);vp.viewportCount=vp.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo rs=MVK_S(PIPELINE_RASTERIZATION_STATE_CREATE_INFO);rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo ms=MVK_S(PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba={0};ba.colorWriteMask=VK_COLOR_COMPONENT_R_BIT;
    VkPipelineColorBlendStateCreateInfo bs=MVK_S(PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);bs.attachmentCount=1;bs.pAttachments=&ba;
    VkDynamicState dyn[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy=MVK_S(PIPELINE_DYNAMIC_STATE_CREATE_INFO);dy.dynamicStateCount=2;dy.pDynamicStates=dyn;
    VkGraphicsPipelineCreateInfo gi=MVK_S(GRAPHICS_PIPELINE_CREATE_INFO);gi.stageCount=2;gi.pStages=stages;gi.pVertexInputState=&vi;gi.pInputAssemblyState=&ia;gi.pViewportState=&vp;gi.pRasterizationState=&rs;gi.pMultisampleState=&ms;gi.pColorBlendState=&bs;gi.pDynamicState=&dy;gi.layout=c->pl;gi.renderPass=c->rp;
    MP_TRY(vkCreateGraphicsPipelines(c->dev,VK_NULL_HANDLE,1,&gi,0,&c->pipe));MP_FAULT(8);
done:
    if(fs)vkDestroyShaderModule(c->dev,fs,0);
    if(vs)vkDestroyShaderModule(c->dev,vs,0);
    if(rc==VK_ERROR_DEVICE_LOST)c->unusable=1;
    if(rc!=VK_SUCCESS)msi_vk_batch_pipeline_destroy(c);
#undef MP_TRY
#undef MP_FAULT
    return rc;
}
static uint32_t mvk_mt(const MsiVkBatchContext *c,uint32_t bits,VkMemoryPropertyFlags flags){
    for(uint32_t i=0;i<c->mem.memoryTypeCount;i++)
        if((bits&(1u<<i))&&(c->mem.memoryTypes[i].propertyFlags&flags)==flags)return i;
    return UINT32_MAX;
}
static VkResult mvk_buffer(const MsiVkBatchContext *c,MsiVkBuffer *b,VkDeviceSize n,VkBufferUsageFlags usage){
    b->size=n;
    VkBufferCreateInfo ci=MVK_S(BUFFER_CREATE_INFO);ci.size=n;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VkResult r=vkCreateBuffer(c->dev,&ci,0,&b->b);if(r!=VK_SUCCESS)return r;
    VkMemoryRequirements m;vkGetBufferMemoryRequirements(c->dev,b->b,&m);
    VkMemoryAllocateInfo a=MVK_S(MEMORY_ALLOCATE_INFO);a.allocationSize=m.size;
    a.memoryTypeIndex=mvk_mt(c,m.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(a.memoryTypeIndex==UINT32_MAX)return VK_ERROR_FEATURE_NOT_PRESENT;
    r=vkAllocateMemory(c->dev,&a,0,&b->m);if(r!=VK_SUCCESS)return r;
    return vkBindBufferMemory(c->dev,b->b,b->m,0);
}
static VkResult mvk_write(const MsiVkBatchContext *c,MsiVkBuffer b,const void *p,size_t n){
    if(n>b.size)return VK_ERROR_INITIALIZATION_FAILED;
    void *mapped=0;VkResult r=vkMapMemory(c->dev,b.m,0,b.size,0,&mapped);
    if(r!=VK_SUCCESS)return r;
    for(size_t i=0;i<n;i++)((uint8_t*)mapped)[i]=((const uint8_t*)p)[i];
    vkUnmapMemory(c->dev,b.m);return VK_SUCCESS;
}
static VkResult mvk_image(const MsiVkBatchContext *c,MsiVkImage *a,uint32_t w,uint32_t h,VkFormat format,VkImageUsageFlags usage){
    VkImageFormatProperties p;VkResult r=vkGetPhysicalDeviceImageFormatProperties(c->phys,format,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,usage,0,&p);
    if(r!=VK_SUCCESS)return r;
    if(p.maxExtent.width<w||p.maxExtent.height<h)return VK_ERROR_FORMAT_NOT_SUPPORTED;
    VkImageCreateInfo ci=MVK_S(IMAGE_CREATE_INFO);ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;
    ci.extent=(VkExtent3D){w,h,1};ci.mipLevels=1;ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;
    ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    r=vkCreateImage(c->dev,&ci,0,&a->i);if(r!=VK_SUCCESS)return r;
    VkMemoryRequirements m;vkGetImageMemoryRequirements(c->dev,a->i,&m);
    VkMemoryAllocateInfo ma=MVK_S(MEMORY_ALLOCATE_INFO);ma.allocationSize=m.size;
    ma.memoryTypeIndex=mvk_mt(c,m.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(ma.memoryTypeIndex==UINT32_MAX)return VK_ERROR_FEATURE_NOT_PRESENT;
    r=vkAllocateMemory(c->dev,&ma,0,&a->m);if(r!=VK_SUCCESS)return r;
    r=vkBindImageMemory(c->dev,a->i,a->m,0);if(r!=VK_SUCCESS)return r;
    VkImageViewCreateInfo vi=MVK_S(IMAGE_VIEW_CREATE_INFO);vi.image=a->i;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;
    vi.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    return vkCreateImageView(c->dev,&vi,0,&a->v);
}
static void mvk_freebuffer(VkDevice d,MsiVkBuffer b){if(b.b)vkDestroyBuffer(d,b.b,0);if(b.m)vkFreeMemory(d,b.m,0);}
static void mvk_freeimage(VkDevice d,MsiVkImage a){if(a.v)vkDestroyImageView(d,a.v,0);if(a.i)vkDestroyImage(d,a.i,0);if(a.m)vkFreeMemory(d,a.m,0);}
static int mvk_cleanup(const MsiVkBatchContext *c,MsiVkBatchWork *k){
    if(k->pending)return 0;
    if(k->cmd)vkFreeCommandBuffers(c->dev,c->cp,1,&k->cmd);
    if(k->fb)vkDestroyFramebuffer(c->dev,k->fb,0);
    if(k->pool)vkDestroyDescriptorPool(c->dev,k->pool,0);
    for(unsigned i=0;i<MSI_GPU_SNAPSHOT_MAX;i++)mvk_freeimage(c->dev,k->textures[i]);
    mvk_freeimage(c->dev,k->out);mvk_freebuffer(c->dev,k->stage);mvk_freebuffer(c->dev,k->vertices);
    mvk_freebuffer(c->dev,k->read);mvk_freebuffer(c->dev,k->initial);
    *k=(MsiVkBatchWork){0};return 1;
}
static void mvk_transition(VkCommandBuffer cmd,VkImage i,VkImageLayout old,VkImageLayout next,VkPipelineStageFlags a,VkPipelineStageFlags b,VkAccessFlags ra,VkAccessFlags rb){
    VkImageMemoryBarrier m=MVK_S(IMAGE_MEMORY_BARRIER);m.image=i;m.oldLayout=old;m.newLayout=next;
    m.srcQueueFamilyIndex=m.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;m.srcAccessMask=ra;m.dstAccessMask=rb;
    m.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    vkCmdPipelineBarrier(cmd,a,b,0,0,0,0,0,1,&m);
}
/* A lost device counts as completion for object lifetime, never for
 * output validity (Vulkan specification, Lost Device). Other wait errors
 * retain pending ownership until a later successful/lost-device wait. */
static VkResult msi_vk_batch_retire(MsiVkBatchContext *c,MsiVkBatchWork *k){
    if(!k->pending)return VK_SUCCESS;
    VkResult r=vkQueueWaitIdle(c->queue);
    if(r!=VK_SUCCESS)c->unusable=1;
    if(r==VK_SUCCESS||r==VK_ERROR_DEVICE_LOST){
        k->pending=0;mvk_cleanup(c,k);
    }
    return r;
}
/* Work must start zeroed. fault=0 normally; 1..6 are deterministic CI
 * acquisition failures. 7/8 simulate wait errors ONLY after real idle. */
static VkResult msi_vk_batch_render(MsiVkBatchContext *c,MsiVkBatchWork *k,
    const MsiGpuDrawBatch *batch,unsigned w,unsigned h,const uint16_t *before,uint16_t *result,unsigned fault){
    if(!c||!k||c->unusable||k->pending||k->pool||!batch||!result||result==before||
       !w||!h||w>4096u||h>4096u||!batch->count||batch->count>MSI_GPU_WAVE_MAX||
       !batch->snapshots.count||batch->snapshots.count>MSI_GPU_SNAPSHOT_MAX||
       !batch->snapshots.decoded||!batch->snapshots.decoded_used||batch->snapshots.decoded_used>2097152u)
        return VK_ERROR_INITIALIZATION_FAILED;
    size_t offset=0;
    for(unsigned i=0;i<batch->snapshots.count;i++){
        const MsiGpuSnapshot *a=&batch->snapshots.entries[i];
        unsigned tw=a->state.width,th=a->state.height;
        if(!tw||!th||tw>2048u||th>2048u||a->decoded_offset!=offset||
           a->decoded_count!=(size_t)tw*th||a->decoded_count>batch->snapshots.decoded_used-offset)
            return VK_ERROR_INITIALIZATION_FAILED;
        offset+=a->decoded_count;
    }
    if(offset!=batch->snapshots.decoded_used)return VK_ERROR_INITIALIZATION_FAILED;
    for(unsigned i=0;i<batch->count;i++)if(batch->draws[i].texture_id>=batch->snapshots.count)return VK_ERROR_INITIALIZATION_FAILED;
    VkResult rc=VK_SUCCESS;
#define MVK_TRY(x) do{rc=(x);if(rc!=VK_SUCCESS)goto done;}while(0)
#define MVK_FAULT(n) do{if(fault==(n)){rc=VK_ERROR_OUT_OF_HOST_MEMORY;goto done;}}while(0)
    VkDescriptorSet sets[MSI_GPU_SNAPSHOT_MAX]={0};VkDescriptorSetLayout layouts[MSI_GPU_SNAPSHOT_MAX];
    VkDescriptorPoolSize ps={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2u*batch->snapshots.count};
    VkDescriptorPoolCreateInfo pi=MVK_S(DESCRIPTOR_POOL_CREATE_INFO);pi.maxSets=batch->snapshots.count;pi.poolSizeCount=1;pi.pPoolSizes=&ps;
    MVK_TRY(vkCreateDescriptorPool(c->dev,&pi,0,&k->pool));MVK_FAULT(1);
    for(unsigned i=0;i<batch->snapshots.count;i++)layouts[i]=c->dsl;
    VkDescriptorSetAllocateInfo da=MVK_S(DESCRIPTOR_SET_ALLOCATE_INFO);da.descriptorPool=k->pool;da.descriptorSetCount=batch->snapshots.count;da.pSetLayouts=layouts;
    MVK_TRY(vkAllocateDescriptorSets(c->dev,&da,sets));
    MVK_TRY(mvk_buffer(c,&k->stage,batch->snapshots.decoded_used*4u,VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    MVK_TRY(mvk_write(c,k->stage,batch->snapshots.decoded,batch->snapshots.decoded_used*4u));MVK_FAULT(2);
    for(unsigned i=0;i<batch->snapshots.count;i++){
        MVK_TRY(mvk_image(c,&k->textures[i],batch->snapshots.entries[i].state.width,batch->snapshots.entries[i].state.height,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT));
        VkDescriptorImageInfo ti[2]={{c->sampler,k->textures[i].v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {c->sampler,k->textures[i].v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
        VkWriteDescriptorSet wr[2]={MVK_S(WRITE_DESCRIPTOR_SET),MVK_S(WRITE_DESCRIPTOR_SET)};
        for(unsigned k=0;k<2;k++){
            wr[k].dstSet=sets[i];wr[k].dstBinding=k;wr[k].descriptorCount=1;
            wr[k].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;wr[k].pImageInfo=&ti[k];
        }
        vkUpdateDescriptorSets(c->dev,2,wr,0,0);
    }
    MVK_FAULT(3);
    MVK_TRY(mvk_buffer(c,&k->vertices,batch->count*3u*sizeof(MsiGpuVertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
    MVK_TRY(mvk_write(c,k->vertices,batch->vertices,batch->count*3u*sizeof(MsiGpuVertex)));
    MVK_TRY(mvk_image(c,&k->out,w,h,VK_FORMAT_R16_UINT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    MVK_TRY(mvk_buffer(c,&k->read,(VkDeviceSize)w*h*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    VkFramebufferCreateInfo fi=MVK_S(FRAMEBUFFER_CREATE_INFO);fi.renderPass=c->rp;fi.attachmentCount=1;fi.pAttachments=&k->out.v;fi.width=w;fi.height=h;fi.layers=1;
    MVK_TRY(vkCreateFramebuffer(c->dev,&fi,0,&k->fb));MVK_FAULT(4);
    VkCommandBufferAllocateInfo ca=MVK_S(COMMAND_BUFFER_ALLOCATE_INFO);ca.commandPool=c->cp;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
    MVK_TRY(vkAllocateCommandBuffers(c->dev,&ca,&k->cmd));
    VkCommandBuffer cmd=k->cmd;VkCommandBufferBeginInfo cb=MVK_S(COMMAND_BUFFER_BEGIN_INFO);cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    MVK_TRY(vkBeginCommandBuffer(cmd,&cb));
    for(unsigned i=0;i<batch->snapshots.count;i++){
        mvk_transition(cmd,k->textures[i].i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy br={0};br.bufferOffset=batch->snapshots.entries[i].decoded_offset*4u;
        br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};br.imageExtent=(VkExtent3D){batch->snapshots.entries[i].state.width,batch->snapshots.entries[i].state.height,1};
        vkCmdCopyBufferToImage(cmd,k->stage.b,k->textures[i].i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&br);
        mvk_transition(cmd,k->textures[i].i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    }
    if(before){
        MVK_TRY(mvk_buffer(c,&k->initial,(VkDeviceSize)w*h*2,VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
        MVK_TRY(mvk_write(c,k->initial,before,(size_t)w*h*2));
        mvk_transition(cmd,k->out.i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy copy={0};copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){w,h,1};
        vkCmdCopyBufferToImage(cmd,k->initial.b,k->out.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        mvk_transition(cmd,k->out.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
    VkClearValue clear={.color.uint32={0xdead,0,0,0}};
    VkRenderPassBeginInfo bi=MVK_S(RENDER_PASS_BEGIN_INFO);bi.renderPass=before?c->rp_load:c->rp;bi.framebuffer=k->fb;bi.renderArea.extent=(VkExtent2D){w,h};if(!before){bi.clearValueCount=1;bi.pClearValues=&clear;}
    vkCmdBeginRenderPass(cmd,&bi,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp={0,0,(float)w,(float)h,0,1};VkRect2D sc={{0,0},{w,h}};
    vkCmdSetViewport(cmd,0,1,&vp);vkCmdSetScissor(cmd,0,1,&sc);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,c->pipe);
    VkDeviceSize off=0;vkCmdBindVertexBuffers(cmd,0,1,&k->vertices.b,&off);
    for(unsigned i=0;i<batch->count;i++){
        const MsiGpuDraw *draw=&batch->draws[i];
        MsiVkPush push={{640,400},{(float)w,(float)h},draw->flags,0};
        vkCmdPushConstants(cmd,c->pl,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,c->pl,0,1,&sets[draw->texture_id],0,0);
        vkCmdDraw(cmd,3,1,i*3u,0);
    }
    vkCmdEndRenderPass(cmd);
    mvk_transition(cmd,k->out.i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy br={0};br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};br.imageExtent=(VkExtent3D){w,h,1};
    vkCmdCopyImageToBuffer(cmd,k->out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,k->read.b,1,&br);
    VkBufferMemoryBarrier bar=MVK_S(BUFFER_MEMORY_BARRIER);bar.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;bar.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    bar.srcQueueFamilyIndex=bar.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bar.buffer=k->read.b;bar.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,0,1,&bar,0,0);
    MVK_TRY(vkEndCommandBuffer(cmd));MVK_FAULT(5);
    VkSubmitInfo submit=MVK_S(SUBMIT_INFO);submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
    MVK_TRY(vkQueueSubmit(c->queue,1,&submit,VK_NULL_HANDLE));k->pending=1;
    rc=vkQueueWaitIdle(c->queue);
    if(rc==VK_SUCCESS&&fault==7)rc=VK_ERROR_OUT_OF_HOST_MEMORY;
    if(rc==VK_SUCCESS&&fault==8)rc=VK_ERROR_DEVICE_LOST;
    if(rc!=VK_SUCCESS){c->unusable=1;goto done;}
    k->pending=0;MVK_FAULT(6);
    void *mapped=0;MVK_TRY(vkMapMemory(c->dev,k->read.m,0,k->read.size,0,&mapped));
    for(size_t i=0;i<(size_t)w*h;i++)result[i]=((const uint16_t*)mapped)[i];
    vkUnmapMemory(c->dev,k->read.m);

done:
    /* The specification permits freeing in-use objects on device loss.
     * Any other failed wait leaves the exact work retained for retirement. */
    if(rc==VK_ERROR_DEVICE_LOST){c->unusable=1;k->pending=0;}
    if(!k->pending)mvk_cleanup(c,k);
#undef MVK_TRY
#undef MVK_FAULT
    return rc;
}
#undef MVK_S
#endif
