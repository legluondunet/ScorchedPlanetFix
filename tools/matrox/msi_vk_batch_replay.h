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
typedef struct {VkBuffer b;VkDeviceMemory m;VkDeviceSize size;void *mapped;} MsiVkBuffer;
typedef struct {VkImage i;VkDeviceMemory m;VkImageView v;} MsiVkImage;
/* One fixed-size native R32 workspace, owned by its pipeline context.
 * Only borrowed by one synchronous cropped submission at a time. */
typedef struct {MsiVkImage out;MsiVkBuffer read;VkFramebuffer fb;} MsiVkRoiCache;
/* Bounded reusable host-coherent uploads for the same cropped path. */
typedef struct {MsiVkBuffer stage,vertices;} MsiVkUploadCache;
enum {MSI_VK_STAGE_MAX=8388608u,MSI_VK_STAGE_MIN=65536u};
/* Cache only single-draw/single-snapshot cropped submissions initially.
 * Exact decoded R32 texels include discard and filter-veto bits. */
enum {MSI_VK_TEX_SLOTS=4u,MSI_VK_TEX_MAX=16384u};
typedef struct {unsigned width,height;uint32_t hash;} MsiVkTextureKey;
typedef struct {MsiVkTextureKey key;MsiVkImage image;MsiVkBuffer reference;} MsiVkTextureEntry;
typedef struct {
    MsiVkTextureEntry entries[MSI_VK_TEX_SLOTS];
    unsigned next,hits,misses,last_mode; /* 0 uncached, 1 hit, 2 miss, 3 unavailable */
} MsiVkTextureCache;
typedef struct {
    VkPhysicalDevice phys;VkDevice dev;VkQueue queue;VkCommandPool cp;
    VkPhysicalDeviceMemoryProperties mem;
    VkDescriptorSetLayout dsl;VkPipelineLayout pl;VkRenderPass rp,rp_load;
    VkPipeline pipe;VkSampler sampler;VkImageView dummy;
    unsigned unusable; /* never reset except by a fresh device context */
    unsigned tagged_output; /* fixed before pipeline creation: R32_UINT, clear=65536 */
    MsiVkRoiCache roi;
    MsiVkUploadCache uploads;
    MsiVkTextureCache texture_cache;
    /* Cropped-only descriptor storage and one idle/reset command buffer.
     * Command ownership moves to work until successful retirement. */
    VkDescriptorPool roi_pool;
    VkDescriptorSet roi_sets[MSI_GPU_SNAPSHOT_MAX];
    VkCommandBuffer roi_cmd;
    VkFence packed_fence; /* unsignaled, idle, only for packed pilot */
    /* Optional owner-clock callback. 0 means no timestamp; UINT32_MAX
     * denotes an unavailable/invalid interval. Host wall time only. */
    uint64_t (*packed_clock)(void);
    uint32_t packed_wait_ticks;
} MsiVkBatchContext;
typedef struct {
    MsiVkImage textures[MSI_GPU_SNAPSHOT_MAX],out;
    MsiVkBuffer stage,vertices,read,initial;
    VkDescriptorPool pool;VkFramebuffer fb;VkCommandBuffer cmd;VkFence fence;
    VkDescriptorSet sets[MSI_GPU_SNAPSHOT_MAX];VkDescriptorSetLayout layouts[MSI_GPU_SNAPSHOT_MAX];
    unsigned pending,roi_borrowed,upload_borrowed,descriptor_borrowed;
    MsiVkBuffer texture_reference; /* owned only on an uncommitted miss */
    MsiVkTextureKey texture_key;
    unsigned texture_slot,texture_hit; /* slot is one-based; 0 = uncached */
} MsiVkBatchWork;
typedef struct {float logical[2],target[2];uint32_t flags,reserved;} MsiVkPush;
static void mvk_freebuffer(VkDevice d,MsiVkBuffer b){
    if(b.mapped)vkUnmapMemory(d,b.m);
    if(b.b)vkDestroyBuffer(d,b.b,0);
    if(b.m)vkFreeMemory(d,b.m,0);
}
static void mvk_freeimage(VkDevice d,MsiVkImage a){if(a.v)vkDestroyImageView(d,a.v,0);if(a.i)vkDestroyImage(d,a.i,0);if(a.m)vkFreeMemory(d,a.m,0);}
static void mvk_roi_destroy(VkDevice d,MsiVkRoiCache *a){
    if(a->fb)vkDestroyFramebuffer(d,a->fb,0);
    mvk_freeimage(d,a->out);mvk_freebuffer(d,a->read);
    *a=(MsiVkRoiCache){0};
}
static void mvk_texture_entry_destroy(VkDevice d,MsiVkTextureEntry *e){
    mvk_freeimage(d,e->image);mvk_freebuffer(d,e->reference);
    *e=(MsiVkTextureEntry){0};
}
/* Persistent pipeline objects are owned by this context; device, queue,
 * command pool and dummy image are borrowed. Destroy only after all work
 * has drained. No device teardown, host allocation or fatal exit here. */
static void msi_vk_batch_pipeline_destroy(MsiVkBatchContext *c){
    /* Caller has retired pending work; release commands/descriptors before
     * referenced resources, framebuffer before render pass. */
    if(c->packed_fence)vkDestroyFence(c->dev,c->packed_fence,0);
    c->packed_fence=VK_NULL_HANDLE;c->packed_clock=0;c->packed_wait_ticks=UINT32_MAX;
    if(c->roi_cmd)vkFreeCommandBuffers(c->dev,c->cp,1,&c->roi_cmd);
    if(c->roi_pool)vkDestroyDescriptorPool(c->dev,c->roi_pool,0);
    c->roi_cmd=VK_NULL_HANDLE;c->roi_pool=VK_NULL_HANDLE;
    for(unsigned i=0;i<MSI_GPU_SNAPSHOT_MAX;i++)c->roi_sets[i]=VK_NULL_HANDLE;
    mvk_roi_destroy(c->dev,&c->roi);
    mvk_freebuffer(c->dev,c->uploads.stage);
    mvk_freebuffer(c->dev,c->uploads.vertices);
    c->uploads=(MsiVkUploadCache){0};
    for(unsigned i=0;i<MSI_VK_TEX_SLOTS;i++)mvk_texture_entry_destroy(c->dev,&c->texture_cache.entries[i]);
    c->texture_cache.next=c->texture_cache.hits=c->texture_cache.misses=0;
    c->texture_cache.last_mode=3u;
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
       c->pipe||c->rp||c->rp_load||c->pl||c->dsl||c->sampler||c->roi.fb)return VK_ERROR_INITIALIZATION_FAILED;
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
    VkAttachmentDescription ad={0};ad.format=c->tagged_output?VK_FORMAT_R32_UINT:VK_FORMAT_R16_UINT;ad.samples=VK_SAMPLE_COUNT_1_BIT;ad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;ad.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
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
/* A CPU-read workload benefits from cached host memory when offered.
 * Never trade away coherence or compatibility with memoryTypeBits. */
static uint32_t mvk_host_mt(const MsiVkBatchContext *c,uint32_t bits,unsigned prefer_cached){
    const VkMemoryPropertyFlags coherent=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if(prefer_cached){
        uint32_t i=mvk_mt(c,bits,coherent|VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        if(i!=UINT32_MAX)return i;
    }
    return mvk_mt(c,bits,coherent);
}
static VkResult mvk_buffer_mode(const MsiVkBatchContext *c,MsiVkBuffer *b,VkDeviceSize n,VkBufferUsageFlags usage,unsigned prefer_cached){
    b->size=n;
    VkBufferCreateInfo ci=MVK_S(BUFFER_CREATE_INFO);ci.size=n;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VkResult r=vkCreateBuffer(c->dev,&ci,0,&b->b);if(r!=VK_SUCCESS)return r;
    VkMemoryRequirements m;vkGetBufferMemoryRequirements(c->dev,b->b,&m);
    VkMemoryAllocateInfo a=MVK_S(MEMORY_ALLOCATE_INFO);a.allocationSize=m.size;
    a.memoryTypeIndex=mvk_host_mt(c,m.memoryTypeBits,prefer_cached);
    if(a.memoryTypeIndex==UINT32_MAX)return VK_ERROR_FEATURE_NOT_PRESENT;
    r=vkAllocateMemory(c->dev,&a,0,&b->m);if(r!=VK_SUCCESS)return r;
    return vkBindBufferMemory(c->dev,b->b,b->m,0);
}
static VkResult mvk_buffer(const MsiVkBatchContext *c,MsiVkBuffer *b,VkDeviceSize n,VkBufferUsageFlags usage){
    return mvk_buffer_mode(c,b,n,usage,0);
}
/* Only cache-owned HOST_VISIBLE | HOST_COHERENT allocations are
 * persistently mapped. Borrowed copies of these handles never unmap.
 * Host writes precede submit; host reads follow successful completion
 * and the existing transfer-to-host barrier. No noncoherent fallback. */
static VkResult mvk_map_cached(const MsiVkBatchContext *c,MsiVkBuffer *b){
    if(!b->m||!b->size||b->mapped)return VK_ERROR_INITIALIZATION_FAILED;
    void *mapped=0;VkResult r=vkMapMemory(c->dev,b->m,0,b->size,0,&mapped);
    if(r==VK_SUCCESS)b->mapped=mapped;
    return r;
}
static VkResult mvk_write(const MsiVkBatchContext *c,MsiVkBuffer b,const void *p,size_t n){
    if(!n||n>b.size)return VK_ERROR_INITIALIZATION_FAILED;
    void *mapped=b.mapped;
    if(!mapped){VkResult r=vkMapMemory(c->dev,b.m,0,n,0,&mapped);if(r!=VK_SUCCESS)return r;}
    for(size_t i=0;i<n;i++)((uint8_t*)mapped)[i]=((const uint8_t*)p)[i];
    if(!b.mapped)vkUnmapMemory(c->dev,b.m);
    return VK_SUCCESS;
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
/* Native tagged-only cache, all-or-nothing creation. Keep an unchanging
 * capacity: only rw*rh texels are copied/mapped, irrespective of allocation.
 * No pending work may borrow it during acquisition or destruction. */
static VkResult mvk_roi_acquire(MsiVkBatchContext *c,MsiVkBatchWork *k,unsigned fault){
    if(!c->roi.fb){
        MsiVkRoiCache a={0};
        VkResult r=mvk_image(c,&a.out,640u,400u,VK_FORMAT_R32_UINT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        if(r==VK_SUCCESS&&fault==9u)r=VK_ERROR_OUT_OF_HOST_MEMORY;
        if(r==VK_SUCCESS)r=mvk_buffer_mode(c,&a.read,640u*400u*4u,VK_BUFFER_USAGE_TRANSFER_DST_BIT,1);
        if(r==VK_SUCCESS&&fault==10u)r=VK_ERROR_OUT_OF_HOST_MEMORY;
        if(r==VK_SUCCESS)r=mvk_map_cached(c,&a.read);
        if(r==VK_SUCCESS&&fault==25u)r=VK_ERROR_MEMORY_MAP_FAILED;
        if(r==VK_SUCCESS){
            VkFramebufferCreateInfo fi=MVK_S(FRAMEBUFFER_CREATE_INFO);
            fi.renderPass=c->rp;fi.attachmentCount=1;fi.pAttachments=&a.out.v;
            fi.width=640u;fi.height=400u;fi.layers=1;
            r=vkCreateFramebuffer(c->dev,&fi,0,&a.fb);
        }
        if(r==VK_SUCCESS&&fault==11u)r=VK_ERROR_OUT_OF_HOST_MEMORY;
        if(r!=VK_SUCCESS){mvk_roi_destroy(c->dev,&a);return r;}
        c->roi=a;
    }
    k->out=c->roi.out;k->read=c->roi.read;k->fb=c->roi.fb;k->roi_borrowed=1;
    return VK_SUCCESS;
}
/* Called only before recording a new synchronous cropped submission.
 * Grow staging geometrically, keep the old buffers on any allocation
 * failure, and copy only the current immutable input bytes. Mappings
 * belong to the same transaction as the replacement allocations. */
static VkResult mvk_upload_acquire(MsiVkBatchContext *c,MsiVkBatchWork *k,
    unsigned stage_bytes,unsigned vertex_bytes,unsigned fault){
    const unsigned vertex_capacity=MSI_GPU_WAVE_MAX*3u*sizeof(MsiGpuVertex);
    if(!stage_bytes||stage_bytes>MSI_VK_STAGE_MAX||!vertex_bytes||vertex_bytes>vertex_capacity||
       k->pending||k->upload_borrowed||k->stage.b||k->vertices.b)
        return VK_ERROR_INITIALIZATION_FAILED;
    MsiVkUploadCache a={0};
    VkResult r=VK_SUCCESS;
    if(c->uploads.stage.size<stage_bytes){
        unsigned capacity=MSI_VK_STAGE_MIN;
        while(capacity<stage_bytes)capacity*=2u;
        r=mvk_buffer(c,&a.stage,capacity,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        if(r==VK_SUCCESS&&fault==12u)r=VK_ERROR_OUT_OF_HOST_MEMORY;
        if(r==VK_SUCCESS)r=mvk_map_cached(c,&a.stage);
        if(r==VK_SUCCESS&&fault==23u)r=VK_ERROR_MEMORY_MAP_FAILED;
    }
    if(r==VK_SUCCESS&&!c->uploads.vertices.b){
        r=mvk_buffer(c,&a.vertices,vertex_capacity,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        if(r==VK_SUCCESS&&fault==13u)r=VK_ERROR_OUT_OF_HOST_MEMORY;
        if(r==VK_SUCCESS)r=mvk_map_cached(c,&a.vertices);
        if(r==VK_SUCCESS&&fault==24u)r=VK_ERROR_MEMORY_MAP_FAILED;
    }
    if(r!=VK_SUCCESS){
        mvk_freebuffer(c->dev,a.stage);mvk_freebuffer(c->dev,a.vertices);
        return r;
    }
    if(a.stage.b){mvk_freebuffer(c->dev,c->uploads.stage);c->uploads.stage=a.stage;}
    if(a.vertices.b)c->uploads.vertices=a.vertices;
    k->stage=c->uploads.stage;k->vertices=c->uploads.vertices;k->upload_borrowed=1;
    return VK_SUCCESS;
}
/* Hash is only a rejection filter. Keep the immutable decoded bytes in
 * their own host-coherent upload buffer, not a guest pointer or batch ID.
 * No host allocation and no assumption about palette/texture lifetimes. */
static uint32_t mvk_texture_hash(const uint32_t *p,size_t n){
    uint32_t h=2166136261u;
    for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;
    return h;
}
static VkResult mvk_texture_prepare(MsiVkBatchContext *c,MsiVkBatchWork *k,
    const MsiGpuDrawBatch *batch,unsigned fault){
    const MsiGpuSnapshot *s=&batch->snapshots.entries[0];
    const uint32_t *src=batch->snapshots.decoded;
    const size_t n=s->decoded_count;
    MsiVkTextureKey key={s->state.width,s->state.height,mvk_texture_hash(src,n)};
    for(unsigned i=0;i<MSI_VK_TEX_SLOTS;i++){
        const MsiVkTextureEntry *e=&c->texture_cache.entries[i];
        if(!e->image.i||e->key.width!=key.width||e->key.height!=key.height||
           e->key.hash!=key.hash||e->reference.size!=n*4u)continue;
        if(!e->reference.mapped)return VK_ERROR_INITIALIZATION_FAILED;
        const uint32_t *old=(const uint32_t*)e->reference.mapped;
        int equal=1;
        for(size_t j=0;j<n;j++)if(old[j]!=src[j]){equal=0;break;}
        if(!equal)continue;
        k->texture_slot=i+1u;k->texture_hit=1;k->textures[0]=e->image;
        return VK_SUCCESS;
    }
    /* Leave the victim intact until the new upload and readback finish.
     * The old image can never be referenced by this miss submission. */
    k->texture_slot=c->texture_cache.next+1u;k->texture_key=key;
    VkResult r=mvk_buffer_mode(c,&k->texture_reference,n*4u,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,1);
    if(r!=VK_SUCCESS)return r;
    if(fault==14u)return VK_ERROR_OUT_OF_HOST_MEMORY;
    r=mvk_map_cached(c,&k->texture_reference);
    if(r!=VK_SUCCESS)return r;
    if(fault==26u)return VK_ERROR_MEMORY_MAP_FAILED;
    return mvk_write(c,k->texture_reference,src,n*4u);
}
static void mvk_texture_commit(MsiVkBatchContext *c,MsiVkBatchWork *k){
    c->texture_cache.last_mode=k->texture_slot?(k->texture_hit?1u:2u):0u;
    if(!k->texture_slot)return;
    if(k->texture_hit){c->texture_cache.hits++;return;}
    unsigned i=k->texture_slot-1u;
    MsiVkTextureEntry *e=&c->texture_cache.entries[i];
    mvk_texture_entry_destroy(c->dev,e);
    e->key=k->texture_key;e->image=k->textures[0];e->reference=k->texture_reference;
    k->textures[0]=(MsiVkImage){0};k->texture_reference=(MsiVkBuffer){0};
    c->texture_cache.next=(i+1u)%MSI_VK_TEX_SLOTS;
    c->texture_cache.misses++;
}
/* Fixed descriptor capacity, allocated transactionally. Each used set is
 * rewritten before recording; stale descriptors in unused sets are never
 * bound. No descriptor indexing, update-after-bind or pool reset. */
static VkResult mvk_descriptor_acquire(MsiVkBatchContext *c,MsiVkBatchWork *k,
    unsigned count,unsigned fault){
    if(!count||count>MSI_GPU_SNAPSHOT_MAX||k->pool||k->pending)
        return VK_ERROR_INITIALIZATION_FAILED;
    if(!c->roi_pool){
        VkDescriptorPool pool=VK_NULL_HANDLE;
        VkDescriptorPoolSize ps={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2u*MSI_GPU_SNAPSHOT_MAX};
        VkDescriptorPoolCreateInfo pi=MVK_S(DESCRIPTOR_POOL_CREATE_INFO);
        pi.maxSets=MSI_GPU_SNAPSHOT_MAX;pi.poolSizeCount=1;pi.pPoolSizes=&ps;
        VkResult r=vkCreateDescriptorPool(c->dev,&pi,0,&pool);
        if(r!=VK_SUCCESS)return r;
        if(fault==16u){vkDestroyDescriptorPool(c->dev,pool,0);return VK_ERROR_OUT_OF_HOST_MEMORY;}
        for(unsigned i=0;i<MSI_GPU_SNAPSHOT_MAX;i++)k->layouts[i]=c->dsl;
        VkDescriptorSetAllocateInfo da=MVK_S(DESCRIPTOR_SET_ALLOCATE_INFO);
        da.descriptorPool=pool;da.descriptorSetCount=MSI_GPU_SNAPSHOT_MAX;da.pSetLayouts=k->layouts;
        r=vkAllocateDescriptorSets(c->dev,&da,k->sets);
        if(r==VK_SUCCESS&&fault==17u)r=VK_ERROR_OUT_OF_HOST_MEMORY;
        if(r!=VK_SUCCESS){vkDestroyDescriptorPool(c->dev,pool,0);return r;}
        c->roi_pool=pool;
        for(unsigned i=0;i<MSI_GPU_SNAPSHOT_MAX;i++)c->roi_sets[i]=k->sets[i];
    }
    k->pool=c->roi_pool;k->descriptor_borrowed=1;
    for(unsigned i=0;i<count;i++)k->sets[i]=c->roi_sets[i];
    return VK_SUCCESS;
}
static int mvk_cleanup(const MsiVkBatchContext *c,MsiVkBatchWork *k){
    if(k->pending)return 0;
    if(k->fence)vkDestroyFence(c->dev,k->fence,0);
    if(k->cmd)vkFreeCommandBuffers(c->dev,c->cp,1,&k->cmd);
    if(k->fb&&!k->roi_borrowed)vkDestroyFramebuffer(c->dev,k->fb,0);
    if(k->pool&&!k->descriptor_borrowed)vkDestroyDescriptorPool(c->dev,k->pool,0);
    for(unsigned i=0;i<MSI_GPU_SNAPSHOT_MAX;i++)if(i||!k->texture_hit)mvk_freeimage(c->dev,k->textures[i]);
    mvk_freebuffer(c->dev,k->texture_reference);
    if(!k->roi_borrowed){mvk_freeimage(c->dev,k->out);mvk_freebuffer(c->dev,k->read);}
    if(!k->upload_borrowed){mvk_freebuffer(c->dev,k->stage);mvk_freebuffer(c->dev,k->vertices);}
    mvk_freebuffer(c->dev,k->initial);
    for(size_t i=0;i<sizeof(*k);i++)((volatile uint8_t*)k)[i]=0;
    return 1;
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
 * acquisition failures. 7/8 simulate wait errors ONLY after real idle.
 * 9..11 inject first-use ROI cache creation failures; 12/13 inject
 * upload-cache allocation failures (including staging growth); 14/15
 * inject immutable texture-cache miss buffer/image failures; 16/17
 * inject persistent descriptor pool/set failures; 18/19 command acquire
 * and post-idle reset failures; 20/21 packed fence acquisition/reset
 * failures; 22 simulates a timeout only after real fence completion.
 * 23..26 inject errors after successful cache mappings (stage, vertices,
 * readback, texture reference) to exercise cleanup of mapped memory.
 * Cropped callers use a reset-capable pool. */
/* Experimental native tagged ROI. Use the union of validated draw bounds
 * plus a one-pixel guard ring to preserve and detect edge disagreements.
 * Do not use this for scaled output or an initial-color LOAD pass. */
static int msi_vk_tagged_region(const MsiGpuDrawBatch *batch,MsiGpuRect *out){
    return msi_gpu_native_tagged_region(batch,out);
}
static VkResult msi_vk_batch_render_impl(MsiVkBatchContext *c,MsiVkBatchWork *k,
    const MsiGpuDrawBatch *batch,unsigned w,unsigned h,const uint16_t *before,void *result,unsigned fault,
    unsigned crop_tagged,size_t packed_capacity,MsiGpuRect *packed_region){
    if(c)c->packed_wait_ticks=UINT32_MAX;
    if(!c||!k||c->unusable||k->pending||k->pool||!batch||!result||result==before||
       (c->tagged_output&&before)||!w||!h||w>4096u||h>4096u||!batch->count||batch->count>MSI_GPU_WAVE_MAX||
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
    for(unsigned i=0;i<batch->count;i++)if(batch->draws[i].texture_id>=batch->snapshots.count||
        (c->tagged_output&&(batch->draws[i].flags&~127u)))return VK_ERROR_INITIALIZATION_FAILED;
    MsiGpuRect roi={0,0,w,h};
    if(crop_tagged&&(!c->tagged_output||w!=640u||h!=400u||!msi_vk_tagged_region(batch,&roi)))
        return VK_ERROR_INITIALIZATION_FAILED;
    unsigned rw=roi.x1-roi.x0,rh=roi.y1-roi.y0;
    if(packed_region&&(!crop_tagged||packed_capacity!=(size_t)rw*rh))
        return VK_ERROR_INITIALIZATION_FAILED;
    c->texture_cache.last_mode=3u;
    VkResult rc=VK_SUCCESS;
#define MVK_TRY(x) do{rc=(x);if(rc!=VK_SUCCESS)goto done;}while(0)
#define MVK_FAULT(n) do{if(fault==(n)){rc=VK_ERROR_OUT_OF_HOST_MEMORY;goto done;}}while(0)
    if(crop_tagged){
        MVK_TRY(mvk_descriptor_acquire(c,k,batch->snapshots.count,fault));MVK_FAULT(1);
    }else{
        VkDescriptorPoolSize ps={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2u*batch->snapshots.count};
        VkDescriptorPoolCreateInfo pi=MVK_S(DESCRIPTOR_POOL_CREATE_INFO);pi.maxSets=batch->snapshots.count;pi.poolSizeCount=1;pi.pPoolSizes=&ps;
        MVK_TRY(vkCreateDescriptorPool(c->dev,&pi,0,&k->pool));MVK_FAULT(1);
        for(unsigned i=0;i<batch->snapshots.count;i++)k->layouts[i]=c->dsl;
        VkDescriptorSetAllocateInfo da=MVK_S(DESCRIPTOR_SET_ALLOCATE_INFO);da.descriptorPool=k->pool;da.descriptorSetCount=batch->snapshots.count;da.pSetLayouts=k->layouts;
        MVK_TRY(vkAllocateDescriptorSets(c->dev,&da,k->sets));
    }
    unsigned stage_bytes=(unsigned)batch->snapshots.decoded_used*4u;
    unsigned vertex_bytes=batch->count*3u*sizeof(MsiGpuVertex);
    if(crop_tagged)MVK_TRY(mvk_upload_acquire(c,k,stage_bytes,vertex_bytes,fault));
    else MVK_TRY(mvk_buffer(c,&k->stage,stage_bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    if(crop_tagged&&batch->count==1u&&batch->snapshots.count==1u&&
       batch->snapshots.decoded_used<=MSI_VK_TEX_MAX)
        MVK_TRY(mvk_texture_prepare(c,k,batch,fault));
    else MVK_TRY(mvk_write(c,k->stage,batch->snapshots.decoded,stage_bytes));
    MVK_FAULT(2);
    for(unsigned i=0;i<batch->snapshots.count;i++){
        if(!k->texture_hit)
            MVK_TRY(mvk_image(c,&k->textures[i],batch->snapshots.entries[i].state.width,batch->snapshots.entries[i].state.height,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT));
        if(k->texture_slot&&!k->texture_hit)MVK_FAULT(15);
        VkDescriptorImageInfo ti[2]={{c->sampler,k->textures[i].v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {c->sampler,k->textures[i].v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
        VkWriteDescriptorSet wr[2]={MVK_S(WRITE_DESCRIPTOR_SET),MVK_S(WRITE_DESCRIPTOR_SET)};
        for(unsigned binding=0;binding<2;binding++){
            wr[binding].dstSet=k->sets[i];wr[binding].dstBinding=binding;wr[binding].descriptorCount=1;
            wr[binding].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;wr[binding].pImageInfo=&ti[binding];
        }
        vkUpdateDescriptorSets(c->dev,2,wr,0,0);
    }
    MVK_FAULT(3);
    if(!crop_tagged)MVK_TRY(mvk_buffer(c,&k->vertices,vertex_bytes,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
    MVK_TRY(mvk_write(c,k->vertices,batch->vertices,vertex_bytes));
    /* Tagged opaque rendering needs no initial-color upload. All actual
     * shader outputs are <=65535, so 65536 uniquely denotes no write. */
    if(crop_tagged){
        /* Previous submission is retired before reuse. The CLEAR render
         * pass begins from UNDEFINED; old ROI contents are irrelevant. */
        MVK_TRY(mvk_roi_acquire(c,k,fault));
    }else{
        MVK_TRY(mvk_image(c,&k->out,w,h,c->tagged_output?VK_FORMAT_R32_UINT:VK_FORMAT_R16_UINT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT));
        MVK_TRY(mvk_buffer(c,&k->read,(VkDeviceSize)rw*rh*(c->tagged_output?4u:2u),VK_BUFFER_USAGE_TRANSFER_DST_BIT));
        VkFramebufferCreateInfo fi=MVK_S(FRAMEBUFFER_CREATE_INFO);fi.renderPass=c->rp;fi.attachmentCount=1;fi.pAttachments=&k->out.v;fi.width=w;fi.height=h;fi.layers=1;
        MVK_TRY(vkCreateFramebuffer(c->dev,&fi,0,&k->fb));
    }
    MVK_FAULT(4);
    VkCommandBufferAllocateInfo ca=MVK_S(COMMAND_BUFFER_ALLOCATE_INFO);ca.commandPool=c->cp;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
    if(crop_tagged&&c->roi_cmd){
        k->cmd=c->roi_cmd;c->roi_cmd=VK_NULL_HANDLE;
    }else MVK_TRY(vkAllocateCommandBuffers(c->dev,&ca,&k->cmd));
    if(crop_tagged)MVK_FAULT(18);
    VkCommandBuffer cmd=k->cmd;VkCommandBufferBeginInfo cb=MVK_S(COMMAND_BUFFER_BEGIN_INFO);cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    MVK_TRY(vkBeginCommandBuffer(cmd,&cb));
    for(unsigned i=0;i<batch->snapshots.count;i++){
        if(k->texture_hit)continue; /* immutable image already in shader-read layout */
        mvk_transition(cmd,k->textures[i].i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy br={0};br.bufferOffset=batch->snapshots.entries[i].decoded_offset*4u;
        br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};br.imageExtent=(VkExtent3D){batch->snapshots.entries[i].state.width,batch->snapshots.entries[i].state.height,1};
        VkBuffer upload=k->texture_slot?k->texture_reference.b:k->stage.b;
        vkCmdCopyBufferToImage(cmd,upload,k->textures[i].i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&br);
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
    VkClearValue clear={.color.uint32={c->tagged_output?65536u:0xdeadu,0,0,0}};
    VkRenderPassBeginInfo bi=MVK_S(RENDER_PASS_BEGIN_INFO);bi.renderPass=before?c->rp_load:c->rp;bi.framebuffer=k->fb;bi.renderArea.offset=(VkOffset2D){(int32_t)roi.x0,(int32_t)roi.y0};bi.renderArea.extent=(VkExtent2D){rw,rh};if(!before){bi.clearValueCount=1;bi.pClearValues=&clear;}
    vkCmdBeginRenderPass(cmd,&bi,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp={0,0,(float)w,(float)h,0,1};VkRect2D sc={{(int32_t)roi.x0,(int32_t)roi.y0},{rw,rh}};
    vkCmdSetViewport(cmd,0,1,&vp);vkCmdSetScissor(cmd,0,1,&sc);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,c->pipe);
    VkDeviceSize off=0;vkCmdBindVertexBuffers(cmd,0,1,&k->vertices.b,&off);
    for(unsigned i=0;i<batch->count;i++){
        const MsiGpuDraw *draw=&batch->draws[i];
        MsiVkPush push={{640,400},{(float)w,(float)h},draw->flags,0};
        vkCmdPushConstants(cmd,c->pl,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,c->pl,0,1,&k->sets[draw->texture_id],0,0);
        vkCmdDraw(cmd,3,1,i*3u,0);
    }
    vkCmdEndRenderPass(cmd);
    mvk_transition(cmd,k->out.i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy br={0};br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    br.imageOffset=(VkOffset3D){(int32_t)roi.x0,(int32_t)roi.y0,0};
    br.imageExtent=(VkExtent3D){rw,rh,1};
    vkCmdCopyImageToBuffer(cmd,k->out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,k->read.b,1,&br);
    VkBufferMemoryBarrier bar=MVK_S(BUFFER_MEMORY_BARRIER);bar.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;bar.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    bar.srcQueueFamilyIndex=bar.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bar.buffer=k->read.b;bar.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,0,1,&bar,0,0);
    MVK_TRY(vkEndCommandBuffer(cmd));MVK_FAULT(5);
    /* Packed single-draw qualification needs only this submission's
     * completion, not a drain of the shared presentation queue.
     * Full/expanded ROI reference paths keep their original idle wait. */
    if(packed_region){
        if(c->packed_fence){
            k->fence=c->packed_fence;c->packed_fence=VK_NULL_HANDLE;
        }else{
            VkFenceCreateInfo fi=MVK_S(FENCE_CREATE_INFO);
            MVK_TRY(vkCreateFence(c->dev,&fi,0,&k->fence));
        }
        MVK_FAULT(20);
    }
    VkSubmitInfo submit=MVK_S(SUBMIT_INFO);submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
    MVK_TRY(vkQueueSubmit(c->queue,1,&submit,k->fence));k->pending=1;
    uint64_t wait_begin=k->fence&&c->packed_clock?c->packed_clock():0;
    rc=k->fence?vkWaitForFences(c->dev,1,&k->fence,VK_TRUE,5000000000ull):vkQueueWaitIdle(c->queue);
    uint64_t wait_end=wait_begin?c->packed_clock():0;
    if(rc==VK_SUCCESS&&k->fence&&fault==22u)rc=VK_TIMEOUT;
    if(rc==VK_SUCCESS&&fault==7)rc=VK_ERROR_OUT_OF_HOST_MEMORY;
    if(rc==VK_SUCCESS&&fault==8)rc=VK_ERROR_DEVICE_LOST;
    if(rc!=VK_SUCCESS){c->unusable=1;goto done;}
    k->pending=0;MVK_FAULT(6);
    if(k->fence){
        /* Only reset/return a completed fence. An uncertain wait retains
         * all resources in work; retirement still drains the queue. */
        MVK_TRY(vkResetFences(c->dev,1,&k->fence));MVK_FAULT(21);
        c->packed_fence=k->fence;k->fence=VK_NULL_HANDLE;
    }
    if(crop_tagged){
        /* Return only a successfully reset, non-pending command. All error
         * paths leave ownership in work, so normal cleanup frees it. */
        MVK_TRY(vkResetCommandBuffer(k->cmd,0));MVK_FAULT(19);
        c->roi_cmd=k->cmd;k->cmd=VK_NULL_HANDLE;
    }
    void *mapped=k->read.mapped;
    if(!mapped)MVK_TRY(vkMapMemory(c->dev,k->read.m,0,(VkDeviceSize)rw*rh*(c->tagged_output?4u:2u),0,&mapped));
    if(c->tagged_output){
        uint32_t *out=(uint32_t*)result;
        const uint32_t *src=(const uint32_t*)mapped;
        if(packed_region){
            for(size_t i=0;i<(size_t)rw*rh;i++)out[i]=src[i];
            *packed_region=roi;
        }else if(crop_tagged){
            /* The cropped render area is cleared to the no-write tag.
             * Outside it no draw can execute because of the scissor.
             * Expand only AFTER a successful wait and mapping. */
            for(size_t i=0;i<(size_t)w*h;i++)out[i]=65536u;
            for(unsigned y=0;y<rh;y++)for(unsigned x=0;x<rw;x++)
                out[(size_t)(roi.y0+y)*w+roi.x0+x]=src[(size_t)y*rw+x];
        }else for(size_t i=0;i<(size_t)w*h;i++)out[i]=src[i];
    }else{
        for(size_t i=0;i<(size_t)w*h;i++)((uint16_t*)result)[i]=((const uint16_t*)mapped)[i];
    }
    if(!k->read.mapped)vkUnmapMemory(c->dev,k->read.m);
    /* Only a successful readback may publish/evict a cache entry. Failed
     * waits retain owned or borrowed resources in work until retirement. */
    mvk_texture_commit(c,k);
    if(wait_begin&&wait_end>=wait_begin&&wait_end-wait_begin<UINT32_MAX)
        c->packed_wait_ticks=(uint32_t)(wait_end-wait_begin);

done:
    /* The specification permits freeing in-use objects on device loss.
     * Any other failed wait leaves the exact work retained for retirement. */
    if(rc==VK_ERROR_DEVICE_LOST){c->unusable=1;k->pending=0;}
    if(!k->pending)mvk_cleanup(c,k);
#undef MVK_TRY
#undef MVK_FAULT
    return rc;
}
/* Full readback stays independent for diagnostic reference comparisons.
 * Native tagged ROI also serves the qualified sparse synchronous pilot. */
static VkResult msi_vk_batch_render(MsiVkBatchContext *c,MsiVkBatchWork *k,
    const MsiGpuDrawBatch *b,unsigned w,unsigned h,const uint16_t *before,void *result,unsigned fault){
    return msi_vk_batch_render_impl(c,k,b,w,h,before,result,fault,0,0,NULL);
}
static VkResult msi_vk_batch_render_tagged_roi(MsiVkBatchContext *c,MsiVkBatchWork *k,
    const MsiGpuDrawBatch *b,uint32_t *result,unsigned fault){
    return msi_vk_batch_render_impl(c,k,b,640u,400u,NULL,result,fault,1,0,NULL);
}
/* Caller supplies exactly the precomputed region capacity. Region and
 * packed tags are untouched on failed acquisition, wait, reset or map. */
static VkResult msi_vk_batch_render_tagged_packed(MsiVkBatchContext *c,MsiVkBatchWork *k,
    const MsiGpuDrawBatch *b,uint32_t *result,size_t capacity,MsiGpuRect *region,unsigned fault){
    if(!region)return VK_ERROR_INITIALIZATION_FAILED;
    return msi_vk_batch_render_impl(c,k,b,640u,400u,NULL,result,fault,1,capacity,region);
}
#undef MVK_S
#endif
