/* SPDX-License-Identifier: GPL-3.0-or-later
 * Standalone offscreen shader proof. No game assets or MSI ABI changes.
 * Run in a fresh process. Fatal errors exit; process owns all Vulkan objects.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <io.h>
#endif
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include "world_spv.h"
#include "msi_world_bilinear.h"
#include "msi_gpu_texel.h"
#include "msi_gpu_batch.h"
#include "msi_gpu_snapshot.h"
#ifdef _WIN32
#include "world_vk_dispatch.h"
#endif
#define OK(x) do { VkResult r_=(x); if(r_!=VK_SUCCESS){fprintf(stderr,"%s: %d\n",#x,r_);exit(1);} }while(0)
#define NEED(x) do {if(!(x)){fprintf(stderr,"check failed: %s\n",#x);exit(1);}}while(0)
#define S(t) {.sType=VK_STRUCTURE_TYPE_##t}
typedef struct {float xy[2],uv[2],iw,m[3];} Vertex;
typedef struct {float logical[2],target[2];uint32_t flags,reserved;} Push;
typedef struct {VkBuffer b;VkDeviceMemory m;VkDeviceSize size;} Buffer;
typedef struct {VkImage i;VkDeviceMemory m;VkImageView v;} Image;
_Static_assert(sizeof(Vertex)==32,"vertex ABI");
_Static_assert(sizeof(Push)==24,"push ABI");
static VkInstance inst;
static VkPhysicalDevice phys;
static VkDevice dev;
static VkQueue queue;
static VkCommandPool cp;
static VkPhysicalDeviceMemoryProperties mem;
static VkDescriptorSetLayout dsl;
static VkPipelineLayout pl;
static VkRenderPass rp,rp_load;
static VkPipeline pipe;
static VkDescriptorPool dp;
static VkDescriptorSet ds;
static VkSampler sampler;
static Image tex;
static Buffer vb,backdrop_vb,upload;
static uint32_t qfamily;
/* Green is discarded; white is visible but forbids filtering (index exception). */
static uint32_t pixels[4]={0xf800,0x107e0,0x001f,0x2ffff};
/* Asymmetric triangle, deliberate negative and >1 UVs, varying invW
 * and lighting. Test far from edges; separately count coverage errors. */
static const Vertex tri[3]={
    {{37,29},{-.37f,.13f},.73f,{241,89,151}},
    {{601,71},{1.61f,-.29f},1.19f,{93,235,174}},
    {{111,363},{.23f,1.47f},1.63f,{167,137,253}}
};
/* Different triangle: overlap and non-overlap with the textured draw. */
static const Vertex backdrop_tri[3]={
    {{8,58},{0,0},1,{0,0,0}},
    {{485,16},{0,0},1,{0,0,0}},
    {{418,385},{0,0},1,{0,0,0}}
};
static uint32_t mt(uint32_t bits,VkMemoryPropertyFlags f){
    for(uint32_t i=0;i<mem.memoryTypeCount;i++)
        if((bits&(1u<<i))&&(mem.memoryTypes[i].propertyFlags&f)==f)return i;
    fprintf(stderr,"no memory type flags=%u\n",f);exit(1);
}
static Buffer buffer(VkDeviceSize n,VkBufferUsageFlags usage){
    Buffer b={0};b.size=n;
    VkBufferCreateInfo ci=S(BUFFER_CREATE_INFO);ci.size=n;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    OK(vkCreateBuffer(dev,&ci,0,&b.b));
    VkMemoryRequirements m;vkGetBufferMemoryRequirements(dev,b.b,&m);
    VkMemoryAllocateInfo a=S(MEMORY_ALLOCATE_INFO);a.allocationSize=m.size;
    a.memoryTypeIndex=mt(m.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    OK(vkAllocateMemory(dev,&a,0,&b.m));OK(vkBindBufferMemory(dev,b.b,b.m,0));return b;
}
static void writebuf(Buffer b,const void *p,size_t n){
    void *d;NEED(n<=b.size);OK(vkMapMemory(dev,b.m,0,b.size,0,&d));memcpy(d,p,n);vkUnmapMemory(dev,b.m);
}
static void freebuf(Buffer b){vkDestroyBuffer(dev,b.b,0);vkFreeMemory(dev,b.m,0);}
static Image image(uint32_t w,uint32_t h,VkFormat format,VkImageUsageFlags usage){
    Image a={0};
    VkImageFormatProperties p;OK(vkGetPhysicalDeviceImageFormatProperties(phys,format,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,usage,0,&p));
    NEED(p.maxExtent.width>=w&&p.maxExtent.height>=h);
    VkImageCreateInfo ci=S(IMAGE_CREATE_INFO);ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;
    ci.extent=(VkExtent3D){w,h,1};ci.mipLevels=1;ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;
    ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    OK(vkCreateImage(dev,&ci,0,&a.i));
    VkMemoryRequirements m;vkGetImageMemoryRequirements(dev,a.i,&m);
    VkMemoryAllocateInfo ma=S(MEMORY_ALLOCATE_INFO);ma.allocationSize=m.size;ma.memoryTypeIndex=mt(m.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    OK(vkAllocateMemory(dev,&ma,0,&a.m));OK(vkBindImageMemory(dev,a.i,a.m,0));
    VkImageViewCreateInfo vi=S(IMAGE_VIEW_CREATE_INFO);vi.image=a.i;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=ci.format;
    vi.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    OK(vkCreateImageView(dev,&vi,0,&a.v));return a;
}
static void freeimage(Image a){vkDestroyImageView(dev,a.v,0);vkDestroyImage(dev,a.i,0);vkFreeMemory(dev,a.m,0);}
static VkCommandBuffer begin(void){
    VkCommandBuffer c;VkCommandBufferAllocateInfo a=S(COMMAND_BUFFER_ALLOCATE_INFO);
    a.commandPool=cp;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;a.commandBufferCount=1;OK(vkAllocateCommandBuffers(dev,&a,&c));
    VkCommandBufferBeginInfo b=S(COMMAND_BUFFER_BEGIN_INFO);b.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;OK(vkBeginCommandBuffer(c,&b));return c;
}
static void end(VkCommandBuffer c){
    OK(vkEndCommandBuffer(c));VkSubmitInfo s=S(SUBMIT_INFO);s.commandBufferCount=1;s.pCommandBuffers=&c;
    OK(vkQueueSubmit(queue,1,&s,VK_NULL_HANDLE));OK(vkQueueWaitIdle(queue));vkFreeCommandBuffers(dev,cp,1,&c);
}
static void transition(VkCommandBuffer c,VkImage i,VkImageLayout old,VkImageLayout next,VkPipelineStageFlags a,VkPipelineStageFlags b,VkAccessFlags ra,VkAccessFlags rb){
    VkImageMemoryBarrier m=S(IMAGE_MEMORY_BARRIER);m.image=i;m.oldLayout=old;m.newLayout=next;
    m.srcQueueFamilyIndex=m.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;m.srcAccessMask=ra;m.dstAccessMask=rb;
    m.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    vkCmdPipelineBarrier(c,a,b,0,0,0,0,0,1,&m);
}
static VkShaderModule shader(const uint32_t *p,size_t n){
    VkShaderModule m;VkShaderModuleCreateInfo a=S(SHADER_MODULE_CREATE_INFO);a.codeSize=n;a.pCode=p;
    OK(vkCreateShaderModule(dev,&a,0,&m));return m;
}
static void setup(void){
    VkApplicationInfo ai=S(APPLICATION_INFO);ai.pApplicationName="Matrox offscreen 3D proof";ai.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci=S(INSTANCE_CREATE_INFO);ci.pApplicationInfo=&ai;OK(vkCreateInstance(&ci,0,&inst));
    uint32_t n=0;OK(vkEnumeratePhysicalDevices(inst,&n,0));NEED(n>0);
    VkPhysicalDevice *p=calloc(n,sizeof(*p));NEED(p);OK(vkEnumeratePhysicalDevices(inst,&n,p));
    for(uint32_t i=0;i<n&&!phys;i++){
#ifdef _WIN32
        VkPhysicalDeviceProperties candidate;vkGetPhysicalDeviceProperties(p[i],&candidate);
        printf("VULKAN_3D_CANDIDATE %s type=%u\n",candidate.deviceName,candidate.deviceType);
        if(candidate.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
#endif
        VkFormatProperties f;
        vkGetPhysicalDeviceFormatProperties(p[i],VK_FORMAT_R16_UINT,&f);
        if((f.optimalTilingFeatures&(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))!=(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))continue;
        vkGetPhysicalDeviceFormatProperties(p[i],VK_FORMAT_R32_UINT,&f);
        if(!(f.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))continue;
        uint32_t q=0;vkGetPhysicalDeviceQueueFamilyProperties(p[i],&q,0);
        VkQueueFamilyProperties *qs=calloc(q,sizeof(*qs));NEED(qs);vkGetPhysicalDeviceQueueFamilyProperties(p[i],&q,qs);
        for(uint32_t j=0;j<q;j++)if(qs[j].queueCount&&(qs[j].queueFlags&VK_QUEUE_GRAPHICS_BIT)){phys=p[i];qfamily=j;break;}
        free(qs);
    }
    free(p);NEED(phys);
    VkPhysicalDeviceProperties prop;vkGetPhysicalDeviceProperties(phys,&prop);
    printf("VULKAN_3D_DEVICE %s type=%u\n",prop.deviceName,prop.deviceType);
    float priority=1;VkDeviceQueueCreateInfo qi=S(DEVICE_QUEUE_CREATE_INFO);qi.queueFamilyIndex=qfamily;qi.queueCount=1;qi.pQueuePriorities=&priority;
    VkDeviceCreateInfo di=S(DEVICE_CREATE_INFO);di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
    OK(vkCreateDevice(phys,&di,0,&dev));vkGetDeviceQueue(dev,qfamily,0,&queue);vkGetPhysicalDeviceMemoryProperties(phys,&mem);
    VkCommandPoolCreateInfo pi=S(COMMAND_POOL_CREATE_INFO);pi.queueFamilyIndex=qfamily;OK(vkCreateCommandPool(dev,&pi,0,&cp));
    tex=image(2,2,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    upload=buffer(sizeof(pixels),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);writebuf(upload,pixels,sizeof(pixels));
    vb=buffer(sizeof(tri),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);writebuf(vb,tri,sizeof(tri));
    backdrop_vb=buffer(sizeof(backdrop_tri),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);writebuf(backdrop_vb,backdrop_tri,sizeof(backdrop_tri));
    VkCommandBuffer c=begin();
    transition(c,tex.i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy copy={0};copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){2,2,1};
    vkCmdCopyBufferToImage(c,upload.b,tex.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    transition(c,tex.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);end(c);
    VkSamplerCreateInfo si=S(SAMPLER_CREATE_INFO);si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.maxLod=0;
    OK(vkCreateSampler(dev,&si,0,&sampler));
    VkDescriptorSetLayoutBinding bindings[2]={
        {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,0},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,0}};
    VkDescriptorSetLayoutCreateInfo dl=S(DESCRIPTOR_SET_LAYOUT_CREATE_INFO);dl.bindingCount=2;dl.pBindings=bindings;OK(vkCreateDescriptorSetLayout(dev,&dl,0,&dsl));
    VkPushConstantRange range={VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push)};
    VkPipelineLayoutCreateInfo li=S(PIPELINE_LAYOUT_CREATE_INFO);li.setLayoutCount=1;li.pSetLayouts=&dsl;li.pushConstantRangeCount=1;li.pPushConstantRanges=&range;
    OK(vkCreatePipelineLayout(dev,&li,0,&pl));
    VkDescriptorPoolSize ps={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2};
    VkDescriptorPoolCreateInfo pd=S(DESCRIPTOR_POOL_CREATE_INFO);pd.maxSets=1;pd.poolSizeCount=1;pd.pPoolSizes=&ps;OK(vkCreateDescriptorPool(dev,&pd,0,&dp));
    VkDescriptorSetAllocateInfo da=S(DESCRIPTOR_SET_ALLOCATE_INFO);da.descriptorPool=dp;da.descriptorSetCount=1;da.pSetLayouts=&dsl;OK(vkAllocateDescriptorSets(dev,&da,&ds));
    VkDescriptorImageInfo ti={sampler,tex.v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr=S(WRITE_DESCRIPTOR_SET);wr.dstSet=ds;wr.descriptorCount=1;wr.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;wr.pImageInfo=&ti;vkUpdateDescriptorSets(dev,1,&wr,0,0);
    wr.dstBinding=1;vkUpdateDescriptorSets(dev,1,&wr,0,0);
    VkAttachmentDescription ad={0};ad.format=VK_FORMAT_R16_UINT;ad.samples=VK_SAMPLE_COUNT_1_BIT;ad.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;ad.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    ad.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;ad.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;ad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;ad.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ar={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub={0};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ar;
    VkRenderPassCreateInfo ri=S(RENDER_PASS_CREATE_INFO);ri.attachmentCount=1;ri.pAttachments=&ad;ri.subpassCount=1;ri.pSubpasses=&sub;OK(vkCreateRenderPass(dev,&ri,0,&rp));
    ad.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;ad.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    OK(vkCreateRenderPass(dev,&ri,0,&rp_load));
    VkShaderModule vs=shader(world_vert,sizeof(world_vert)),fs=shader(world_frag,sizeof(world_frag));
    VkPipelineShaderStageCreateInfo stages[2]={S(PIPELINE_SHADER_STAGE_CREATE_INFO),S(PIPELINE_SHADER_STAGE_CREATE_INFO)};
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[0].pName="main";stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;stages[1].pName="main";
    VkVertexInputBindingDescription bd={0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[]={{0,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,xy)},{1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)},{2,0,VK_FORMAT_R32_SFLOAT,offsetof(Vertex,iw)},{3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,m)}};
    VkPipelineVertexInputStateCreateInfo vi=S(PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&bd;vi.vertexAttributeDescriptionCount=4;vi.pVertexAttributeDescriptions=attrs;
    VkPipelineInputAssemblyStateCreateInfo ia=S(PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp=S(PIPELINE_VIEWPORT_STATE_CREATE_INFO);vp.viewportCount=vp.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo rs=S(PIPELINE_RASTERIZATION_STATE_CREATE_INFO);rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo ms=S(PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba={0};ba.colorWriteMask=VK_COLOR_COMPONENT_R_BIT;
    VkPipelineColorBlendStateCreateInfo bs=S(PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);bs.attachmentCount=1;bs.pAttachments=&ba;
    VkDynamicState dyn[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy=S(PIPELINE_DYNAMIC_STATE_CREATE_INFO);dy.dynamicStateCount=2;dy.pDynamicStates=dyn;
    VkGraphicsPipelineCreateInfo gi=S(GRAPHICS_PIPELINE_CREATE_INFO);gi.stageCount=2;gi.pStages=stages;gi.pVertexInputState=&vi;gi.pInputAssemblyState=&ia;gi.pViewportState=&vp;gi.pRasterizationState=&rs;gi.pMultisampleState=&ms;gi.pColorBlendState=&bs;gi.pDynamicState=&dy;gi.layout=pl;gi.renderPass=rp;
    OK(vkCreateGraphicsPipelines(dev,VK_NULL_HANDLE,1,&gi,0,&pipe));vkDestroyShaderModule(dev,vs,0);vkDestroyShaderModule(dev,fs,0);
}
/* Replace the sampled image contents only after prior draws have completed.
 * The independent expected packed array drives the CPU oracle; the GPU
 * receives bytes decoded from original-style source and palette fixtures. */
static void replace_fixture(const MsiGpuTexture *s,const uint8_t *src,size_t src_size,
    const uint8_t *lut,size_t lut_size,const uint32_t expected[4]){
    uint32_t decoded[4];NEED(msi_gpu_decode_world(s,src,src_size,lut,lut_size,decoded,4));
    NEED(memcmp(decoded,expected,sizeof(decoded))==0);
    memcpy(pixels,expected,sizeof(pixels));writebuf(upload,decoded,sizeof(decoded));
    VkCommandBuffer c=begin();
    transition(c,tex.i,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy copy={0};copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){2,2,1};
    vkCmdCopyBufferToImage(c,upload.b,tex.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    transition(c,tex.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);end(c);
    printf("VULKAN_3D_FIXTURE decoded_planes=%u\n",s->planes);
}
static double cross(double a,double b,double c,double d){return a*d-b*c;}
static void weights_for(const Vertex *v,double x,double y,double *w){
    double ax=v[0].xy[0],ay=v[0].xy[1],bx=v[1].xy[0]-ax,by=v[1].xy[1]-ay,cx=v[2].xy[0]-ax,cy=v[2].xy[1]-ay;
    double den=cross(bx,by,cx,cy);w[1]=cross(x-ax,y-ay,cx,cy)/den;w[2]=cross(bx,by,x-ax,y-ay)/den;w[0]=1-w[1]-w[2];
}
static void weights(double x,double y,double *w){weights_for(tri,x,y,w);}
static double coverage_min(const double *w){return fmin(w[0],fmin(w[1],w[2]));}
static int addr(int i,int clamp){return clamp?(i<0?0:i>1?1:i):((i%2)+2)%2;}
static unsigned tap(int x,int y,unsigned f){return pixels[addr(y,f&4)*2+addr(x,f&2)];}
static double coord(double u,int clamp){return clamp?(u<0?0:u>1?1:u):u-floor(u);}
static unsigned mod(unsigned v,unsigned m,unsigned threshold,int dither){
    unsigned n=v*m;return dither?n/255+(n%255>threshold):(n+127)/255;
}
static uint16_t refpixel_offset(double *w,double x,double y,unsigned f,double du,double dv){
    double u=0,v=0,iw=0,m[3]={0};
    for(int i=0;i<3;i++){double t=w[i]*((f&1)?tri[i].iw:1);iw+=t;u+=t*tri[i].uv[0];v+=t*tri[i].uv[1];for(int k=0;k<3;k++)m[k]+=w[i]*tri[i].m[k];}
    u=coord(u/iw+du,f&2);v=coord(v/iw+dv,f&4);
    int nx=(int)(u*2),ny=(int)(v*2);
    unsigned nearest=tap(nx>1?1:nx,ny>1?1:ny,f);
    if((f&64)&&(nearest&0x10000u))return 0xdead;
    unsigned color=nearest&65535u;
    if(f&8){double sx=u*2-.5,sy=v*2-.5;int ix=(int)floor(sx),iy=(int)floor(sy);
        unsigned wx=(unsigned)((sx-ix)*256+.5),wy=(unsigned)((sy-iy)*256+.5);
        unsigned a=tap(ix,iy,f),b=tap(ix+1,iy,f),c=tap(ix,iy+1,f),d=tap(ix+1,iy+1,f);
        if(!(f&64)||!((a|b|c|d)&0x30000u))
            color=msi_bilinear_mix_rgb565(a&65535u,b&65535u,c&65535u,d&65535u,wx,wy);
    }
    if(f&16){static const unsigned bayer[]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
        unsigned rank=bayer[((unsigned)y&3)*4+((unsigned)x&3)],t=((2*rank+1)*255)/32;
        unsigned r=mod(color>>11,(unsigned)(m[0]+.5),t,f&32),g=mod((color>>5)&63,(unsigned)(m[1]+.5),t,f&32),b=mod(color&31,(unsigned)(m[2]+.5),t,f&32);
        color=(r<<11)|(g<<5)|b;
    }
    return (uint16_t)color;
}
static uint16_t refpixel(double *w,double x,double y,unsigned f){return refpixel_offset(w,x,y,f,0,0);}
/* Only accept exact alternate texel colors within a tiny normalized UV
 * interval at nearest-sampling discontinuities in this 2x2 fixture. */
static int nearest_boundary_match(double *w,double x,double y,unsigned f,uint16_t actual){
    if((f&8)&&!(f&64))return 0;
    const double eps=0.000002;
    uint16_t base=refpixel(w,x,y,f);
    for(int i=-1;i<=1;i++)for(int j=-1;j<=1;j++){
        if(!i&&!j)continue;
        uint16_t alt=refpixel_offset(w,x,y,f,i*eps,j*eps);
        /* For keyed bilinear, only allow a real discard discontinuity. */
        if((f&8)&&base!=0xdead&&alt!=0xdead)continue;
        if(alt!=base&&actual==alt)return 1;
    }
    return 0;
}
static unsigned logical_pixel(unsigned pixel,unsigned logical,unsigned target){
    return (2u*pixel+1u)*logical/(2u*target);
}
static void oracle_selftest(void){
    NEED(logical_pixel(706,640,1440)==314);
    NEED(logical_pixel(407,400,1080)==150);
    NEED(logical_pixel(13,400,1080)==5);
    for(unsigned i=0;i<640;i++)NEED(logical_pixel(i,640,640)==i);
    for(unsigned i=0;i<400;i++)NEED(logical_pixel(i,400,400)==i);
    double w[3],interior_weights[3];
    double x=1083.5*640/1440,y=295.5*400/1080;
    weights(x,y,w);weights(200.5,150.5,interior_weights);
    NEED(refpixel(w,x,y,0)==0x001f);
    NEED(nearest_boundary_match(w,x,y,0,0xf800));
    NEED(!nearest_boundary_match(w,x,y,0,0x07e0));
    NEED(!nearest_boundary_match(w,x,y,8,0xf800));
    NEED(!nearest_boundary_match(interior_weights,200.5,150.5,0,0xdead));
    x=706.5*640/1440;y=407.5*400/1080;weights(x,y,w);
    NEED(refpixel(w,314,150,63)==0x554d);
    NEED(refpixel_offset(w,313,150,63,0,-0.000002)==0x4d4b);
    NEED(refpixel_offset(w,314,150,63,0,-0.000002)==0x4d4c);
    /* Direct barycentric samples at the vertices: fixed wrapped UVs.
       Compare host metadata independently of the shader. */
    NEED(tap(1,0,64)==0x107e0);
    NEED(tap(1,1,64)==0x2ffff);
    NEED((tap(1,1,64)&0x10000u)==0);
    puts("VULKAN_3D_ORACLE_SELFTEST_PASS");
}
static int absdiff(unsigned a,unsigned b){return a>b?(int)(a-b):(int)(b-a);}
static int channel_error(uint16_t a,uint16_t b){
    int r=absdiff(a>>11,b>>11),g=absdiff((a>>5)&63,(b>>5)&63),bl=absdiff(a&31,b&31);
    return r>g?(r>bl?r:bl):(g>bl?g:bl);
}
#define BACKDROP 0x39e7u
/* The CPU oracle applies the existing MSI565 integer source-over rule.
 * Discard must preserve previous color exactly, not produce alpha halos. */
static uint16_t compose_reference(uint16_t src,unsigned opacity,uint16_t old){
    if(src==0xdead||!opacity)return old;
    unsigned inv=100u-opacity;
    unsigned r=(((src>>11)*opacity+(old>>11)*inv)+50u)/100u;
    unsigned g=((((src>>5)&63u)*opacity+((old>>5)&63u)*inv)+50u)/100u;
    unsigned b=(((src&31u)*opacity+(old&31u)*inv)+50u)/100u;
    return (uint16_t)((r<<11)|(g<<5)|b);
}
static int composite_boundary_match(double *w,unsigned x,unsigned y,unsigned flags,unsigned opacity,uint16_t old,uint16_t actual){
    const double eps=0.000002;
    uint16_t base=refpixel(w,x,y,flags);
    for(int i=-1;i<=1;i++)for(int j=-1;j<=1;j++){
        if(!i&&!j)continue;
        uint16_t alt=refpixel_offset(w,x,y,flags,i*eps,j*eps);
        if((flags&8)&&base!=0xdead&&alt!=0xdead)continue;
        if(alt!=base&&compose_reference(alt,opacity,old)==actual)return 1;
    }
    return 0;
}
static void bind_previous(Image a){
    VkDescriptorImageInfo ti={sampler,a.v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr=S(WRITE_DESCRIPTOR_SET);wr.dstSet=ds;wr.dstBinding=1;
    wr.descriptorCount=1;wr.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;wr.pImageInfo=&ti;
    vkUpdateDescriptorSets(dev,1,&wr,0,0);
}
static void draw_pass(VkCommandBuffer c,VkFramebuffer fb,VkRenderPass pass,uint32_t w,uint32_t h,unsigned flags,unsigned reserved,Buffer vertices){
    VkClearValue clear={.color.uint32={0xdead,0,0,0}};
    VkRenderPassBeginInfo bi=S(RENDER_PASS_BEGIN_INFO);bi.renderPass=pass;bi.framebuffer=fb;bi.renderArea.extent=(VkExtent2D){w,h};
    if(pass==rp){bi.clearValueCount=1;bi.pClearValues=&clear;}
    vkCmdBeginRenderPass(c,&bi,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp={0,0,(float)w,(float)h,0,1};VkRect2D sc={{0,0},{w,h}};
    vkCmdSetViewport(c,0,1,&vp);vkCmdSetScissor(c,0,1,&sc);
    vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pipe);vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pl,0,1,&ds,0,0);
    VkDeviceSize offset=0;vkCmdBindVertexBuffers(c,0,1,&vertices.b,&offset);
    Push push={{640,400},{(float)w,(float)h},flags,reserved};
    vkCmdPushConstants(c,pl,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
    vkCmdDraw(c,3,1,0,0);vkCmdEndRenderPass(c);
}
#define CPU_PATCH_COLOR 0x9b26u
static int in_cpu_patch(unsigned x,unsigned y){return x>=24&&x<210&&y>=100&&y<200;}
static int test_case(uint32_t w,uint32_t h,unsigned flags,int opacity,unsigned sync_mode){
    NEED(w>0&&h>0&&w<=16384&&h<=16384);
    Image out=image(w,h,VK_FORMAT_R16_UINT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    Buffer read=buffer((VkDeviceSize)w*h*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    unsigned cpu_checked=0,cpu_bad=0,cpu_written=0;
    VkFramebuffer fb;VkFramebufferCreateInfo fi=S(FRAMEBUFFER_CREATE_INFO);fi.renderPass=rp;fi.attachmentCount=1;fi.pAttachments=&out.v;fi.width=w;fi.height=h;fi.layers=1;OK(vkCreateFramebuffer(dev,&fi,0,&fb));
    Image prior={0};
    if(opacity>=0){
        prior=image(w,h,VK_FORMAT_R16_UINT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        /* First draw opaque background into the actual target. Binding 1
         * still points to dummy tex, never the active attachment. */
        VkCommandBuffer first=begin();draw_pass(first,fb,rp,w,h,128,BACKDROP,sync_mode?backdrop_vb:vb);
        transition(first,out.i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        if(sync_mode==2){
            VkBufferImageCopy br={0};br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};br.imageExtent=(VkExtent3D){w,h,1};
            vkCmdCopyImageToBuffer(first,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read.b,1,&br);
            VkBufferMemoryBarrier b=S(BUFFER_MEMORY_BARRIER);b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT|VK_ACCESS_HOST_WRITE_BIT;
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.buffer=read.b;b.size=VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(first,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,0,1,&b,0,0);end(first);
            /* Simulate the guest reading native color then editing it directly.
             * Preserve untouched pixels, including both sides of the first
             * triangle. This is a synthetic barrier proof, not the live ABI. */
            void *mapped;OK(vkMapMemory(dev,read.m,0,read.size,0,&mapped));
            uint16_t *cpu=mapped;
            for(uint32_t y=0;y<h;y++)for(uint32_t x=0;x<w;x++){
                double bw[3];weights_for(backdrop_tri,(x+.5)*640/w,(y+.5)*400/h,bw);
                double mn=coverage_min(bw);size_t i=(size_t)y*w+x;
                if(mn>.002||mn<-.002){cpu_checked++;if(cpu[i]!=(mn>.002?BACKDROP:0xdead))cpu_bad++;}
                if(in_cpu_patch(logical_pixel(x,640,w),logical_pixel(y,400,h))){cpu[i]=CPU_PATCH_COLOR;cpu_written++;}
            }
            vkUnmapMemory(dev,read.m);
            first=begin();
            transition(first,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
            vkCmdCopyBufferToImage(first,read.b,out.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&br);
            transition(first,out.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        }
        transition(first,prior.i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageCopy region={0};region.srcSubresource=region.dstSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.extent=(VkExtent3D){w,h,1};
        vkCmdCopyImage(first,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,prior.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
        transition(first,prior.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
        transition(first,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        end(first);bind_previous(prior);
    }
    VkCommandBuffer c=begin();
    draw_pass(c,fb,opacity<0?rp:rp_load,w,h,flags|(opacity<0?0u:256u),opacity<0?0u:(unsigned)opacity,vb);
    transition(c,out.i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy copy={0};copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){w,h,1};vkCmdCopyImageToBuffer(c,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read.b,1,&copy);
    VkBufferMemoryBarrier bar=S(BUFFER_MEMORY_BARRIER);bar.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;bar.dstAccessMask=VK_ACCESS_HOST_READ_BIT;bar.srcQueueFamilyIndex=bar.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bar.buffer=read.b;bar.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,0,1,&bar,0,0);end(c);
    void *mapped;OK(vkMapMemory(dev,read.m,0,read.size,0,&mapped));const uint16_t *p=mapped;
    unsigned interior=0,exact=0,one=0,boundary=0,bad=0,outside=0,coverage=0,edges=0,discarded=0,overlap=0,nonoverlap=0,patched_outside=0;
    for(uint32_t y=0;y<h;y++)for(uint32_t x=0;x<w;x++){
        double lx=(x+.5)*640/w,ly=(y+.5)*400/h,b[3];weights(lx,ly,b);
        double mn=fmin(b[0],fmin(b[1],b[2]));uint16_t actual=p[(size_t)y*w+x];
        unsigned dx=logical_pixel(x,640,w),dy=logical_pixel(y,400,h);
        uint16_t old=opacity>=0?BACKDROP:0xdead;
        uint16_t outside_ref=0xdead;
        if(sync_mode){
            double bw[3];weights_for(backdrop_tri,lx,ly,bw);
            double bm=coverage_min(bw);
            int patch=sync_mode==2&&in_cpu_patch(dx,dy);
            if(!patch&&bm>=-.002&&bm<=.002){edges++;continue;}
            old=patch?CPU_PATCH_COLOR:bm>.002?BACKDROP:0xdead;
            outside_ref=old;
            if(mn>.002){if(bm>.002)overlap++;else if(bm<-.002)nonoverlap++;}
            if(patch&&mn<-.002)patched_outside++;
        }
        /* exclude a thin geometric edge band from strict interior checks */
        if(mn>.002){
            interior++;
            uint16_t raw=refpixel(b,dx,dy,flags);
            int keyed_discard=(flags&64)&&raw==0xdead;
            if(keyed_discard)discarded++;
            uint16_t ref=opacity<0?raw:compose_reference(raw,(unsigned)opacity,old);
            int e=channel_error(actual,ref);
            if(!e)exact++;
            else if(e==1&&!keyed_discard&&(opacity<0?actual!=0xdead:opacity>0))one++;
            else if(opacity<0?nearest_boundary_match(b,dx,dy,flags,actual):
                composite_boundary_match(b,dx,dy,flags,(unsigned)opacity,old,actual)){
                boundary++;if(boundary<=3)printf("nearest boundary %u,%u expected=%04x got=%04x\n",x,y,ref,actual);
            }else{if(bad<3)fprintf(stderr,"pixel %u,%u expected=%04x got=%04x\n",x,y,ref,actual);bad++;}
        }
        else if(mn<-.002){outside++;if(actual!=outside_ref)coverage++;}
        else edges++;
    }
    printf("VULKAN_3D_CASE %ux%u flags=%u opacity=%d interior=%u exact=%u one_lsb=%u nearest_boundary=%u bad=%u outside=%u coverage_errors=%u edge_band=%u discarded=%u\n",w,h,flags,opacity,interior,exact,one,boundary,bad,outside,coverage,edges,discarded);
    if(sync_mode)printf("VULKAN_3D_ORDER mode=%u overlap=%u nonoverlap=%u cpu_checked=%u cpu_bad=%u cpu_written=%u patched_outside=%u\n",
        sync_mode,overlap,nonoverlap,cpu_checked,cpu_bad,cpu_written,patched_outside);
    vkUnmapMemory(dev,read.m);vkDestroyFramebuffer(dev,fb,0);freeimage(out);freebuf(read);
    if(opacity>=0){bind_previous(tex);freeimage(prior);}
    /* Keep strict error checks, but collect all cases before returning. */
    return interior>10000&&outside>10000&&coverage==0&&bad==0&&exact*100u>=interior*99u&&boundary*10000u<=interior&&
        (!(flags&64)||(discarded>1000&&interior-discarded>1000))&&
        (!sync_mode||(overlap>1000&&nonoverlap>1000))&&
        (sync_mode!=2||(cpu_bad==0&&cpu_checked>10000&&cpu_written>1000&&patched_outside>1000));
}
static int test(uint32_t w,uint32_t h,unsigned flags){return test_case(w,h,flags,-1,0);}

/* Conservative disjoint bounding boxes: one snapshot per wave rather than
 * one per primitive. All commands recorded before the single submission.
 * Deliberately simple solid-color fixture: texture descriptor batching and
 * ownership of actual game shadows are not claimed here. */
#define BATCH_COUNT 40u
static unsigned batch_color(unsigned i){return ((4u+i%24u)<<11)|((17u+i%31u)<<5)|(3u+i%23u);}
static uint16_t batch_blend(uint16_t src,uint16_t dst,unsigned opacity){
    unsigned inv=100u-opacity;
    return (uint16_t)(((((src>>11)*opacity+(dst>>11)*inv+50u)/100u)<<11)|
        (((((src>>5)&63u)*opacity+((dst>>5)&63u)*inv+50u)/100u)<<5)|
        (((src&31u)*opacity+(dst&31u)*inv+50u)/100u));
}
static void batch_draw(VkCommandBuffer cmd,VkFramebuffer fb,VkRenderPass pass,
    Buffer vertices,unsigned w,unsigned h,unsigned wave){
    VkClearValue clear={.color.uint32={0xdead,0,0,0}};
    VkRenderPassBeginInfo bi=S(RENDER_PASS_BEGIN_INFO);bi.renderPass=pass;bi.framebuffer=fb;
    bi.renderArea.extent=(VkExtent2D){w,h};if(pass==rp){bi.clearValueCount=1;bi.pClearValues=&clear;}
    vkCmdBeginRenderPass(cmd,&bi,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp={0,0,(float)w,(float)h,0,1};VkRect2D sc={{0,0},{w,h}};
    vkCmdSetViewport(cmd,0,1,&vp);vkCmdSetScissor(cmd,0,1,&sc);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipe);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pl,0,1,&ds,0,0);
    VkDeviceSize off=0;vkCmdBindVertexBuffers(cmd,0,1,&vertices.b,&off);
    for(unsigned i=0;i<BATCH_COUNT;i++){
        if(wave==2&&(i&1u))continue;
        unsigned color=wave==0?batch_color(i):wave==1?0x0841u:0xf800u;
        unsigned opacity=wave==1?50u:25u;
        Push push={{640,400},{(float)w,(float)h},wave?384u:128u,color|(wave?opacity<<16u:0u)};
        vkCmdPushConstants(cmd,pl,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
        vkCmdDraw(cmd,3,1,i*3u,0);
    }
    vkCmdEndRenderPass(cmd);
}
static unsigned batch_snapshot(VkCommandBuffer cmd,Image out,Image prior,
    unsigned w,unsigned h,unsigned wave){
    transition(cmd,out.i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    transition(cmd,prior.i,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy copies[BATCH_COUNT]={0};unsigned n=0,total=0;
    MsiGpuWave planned={0};msi_gpu_wave_reset(&planned);
    for(unsigned i=0;i<BATCH_COUNT;i++){
        if(wave==2&&(i&1u))continue;
        unsigned lx=(i%8u)*80u+8u,ly=(i/8u)*80u+8u;
        unsigned x0=lx*w/640u,y0=ly*h/400u;
        unsigned x1=((lx+32u)*w+639u)/640u,y1=((ly+32u)*h+399u)/400u;
        NEED(msi_gpu_wave_push(&planned,(MsiGpuRect){x0,y0,x1,y1},1)==MSI_GPU_WAVE_ACCEPT);
        VkImageCopy *r=&copies[n++];
        r->srcSubresource=r->dstSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        r->srcOffset=r->dstOffset=(VkOffset3D){(int32_t)x0,(int32_t)y0,0};
        r->extent=(VkExtent3D){x1-x0,y1-y0,1};total+=(x1-x0)*(y1-y0);
    }
    NEED(planned.count==n);
    vkCmdCopyImage(cmd,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,prior.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,n,copies);
    transition(cmd,prior.i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    transition(cmd,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    return total;
}
static int test_batch(unsigned w,unsigned h){
    Vertex verts[BATCH_COUNT*3u]={0};
    for(unsigned i=0;i<BATCH_COUNT;i++){
        float x=(float)((i%8u)*80u+8u),y=(float)((i/8u)*80u+8u);
        verts[3*i].xy[0]=x;verts[3*i].xy[1]=y;
        verts[3*i+1].xy[0]=x+32;verts[3*i+1].xy[1]=y;
        verts[3*i+2].xy[0]=x;verts[3*i+2].xy[1]=y+32;
    }
    Buffer vertices=buffer(sizeof(verts),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);writebuf(vertices,verts,sizeof(verts));
    Image out=image(w,h,VK_FORMAT_R16_UINT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    Image prior=image(w,h,VK_FORMAT_R16_UINT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    Buffer read=buffer((VkDeviceSize)w*h*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkFramebuffer fb;VkFramebufferCreateInfo fi=S(FRAMEBUFFER_CREATE_INFO);fi.renderPass=rp;fi.attachmentCount=1;fi.pAttachments=&out.v;fi.width=w;fi.height=h;fi.layers=1;
    OK(vkCreateFramebuffer(dev,&fi,0,&fb));bind_previous(prior);
    VkCommandBuffer cmd=begin();
    transition(cmd,prior.i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,VK_ACCESS_SHADER_READ_BIT);
    batch_draw(cmd,fb,rp,vertices,w,h,0);
    unsigned copied=batch_snapshot(cmd,out,prior,w,h,1);
    batch_draw(cmd,fb,rp_load,vertices,w,h,1);
    copied+=batch_snapshot(cmd,out,prior,w,h,2);
    batch_draw(cmd,fb,rp_load,vertices,w,h,2);
    transition(cmd,out.i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy br={0};br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};br.imageExtent=(VkExtent3D){w,h,1};
    vkCmdCopyImageToBuffer(cmd,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read.b,1,&br);
    VkBufferMemoryBarrier bar=S(BUFFER_MEMORY_BARRIER);bar.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;bar.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    bar.srcQueueFamilyIndex=bar.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bar.buffer=read.b;bar.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,0,1,&bar,0,0);
    end(cmd);
    void *mapped;OK(vkMapMemory(dev,read.m,0,read.size,0,&mapped));const uint16_t *p=mapped;
    unsigned checked=0,errors=0,inside=0,edges=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        double lx=(x+.5)*640/w,ly=(y+.5)*400/h;
        unsigned tile=(unsigned)(ly/80)*8u+(unsigned)(lx/80);
        double a=lx-(double)((tile%8u)*80u+8u),b=ly-(double)((tile/8u)*80u+8u);
        double m=fmin(a,fmin(b,32-a-b));
        if(fabs(m)<.1){edges++;continue;}
        uint16_t want=0xdead;
        if(m>0){inside++;want=batch_blend(0x0841u,(uint16_t)batch_color(tile),50);
            if(!(tile&1u))want=batch_blend(0xf800u,want,25);}
        checked++;
        if(p[(size_t)y*w+x]!=want){if(errors<3)fprintf(stderr,"batch pixel %u,%u expected=%04x got=%04x\n",x,y,want,p[(size_t)y*w+x]);errors++;}
    }
    printf("VULKAN_3D_BATCH %ux%u draws=100 submissions=1 snapshot_calls=2 copied_pixels=%u full_snapshot_pixels=%u checked=%u inside=%u edge_band=%u errors=%u\n",
        w,h,copied,2u*w*h,checked,inside,edges,errors);
    vkUnmapMemory(dev,read.m);bind_previous(tex);vkDestroyFramebuffer(dev,fb,0);
    freeimage(prior);freeimage(out);freebuf(vertices);freebuf(read);
    return checked>10000&&inside>1000&&errors==0&&copied<2u*w*h;
}


/* A single submission with independent immutable descriptor sets. The same
 * indexed texture address is reused by the CPU while recording. Palette and
 * source changes must not retroactively change any earlier draw. */
#define MULTI_N 16u
static uint8_t multi_raw[MULTI_N*516u];
static uint32_t multi_decoded[MULTI_N*4u];
static MsiGpuSnapshots multi_cache;
static void set_palette_color(uint8_t *p,unsigned idx,unsigned color){
    p[2u*idx]=(uint8_t)color;p[2u*idx+1u]=(uint8_t)(color>>8);
}
static int test_indexed_batch(unsigned w,unsigned h){
    MsiGpuTexture state={2,2,8,1,0,0xffff,1,0,0};
    uint8_t source[4]={1,0,1,1},palette[512]={0};
    multi_cache.raw=multi_raw;multi_cache.raw_capacity=sizeof(multi_raw);
    multi_cache.decoded=multi_decoded;multi_cache.decoded_capacity=sizeof(multi_decoded)/sizeof(multi_decoded[0]);
    msi_gpu_snapshot_reset(&multi_cache);
    /* Keyed green is never drawn. Blue remains available for direct writes. */
    set_palette_color(palette,0,0x07e0);
    set_palette_color(palette,2,0x001f);
    Vertex verts[MULTI_N*3u]={0};
    unsigned ids[MULTI_N],expected[MULTI_N],hits=0,misses=0;
    for(unsigned i=0;i<MULTI_N;i++){
        unsigned pattern=i%4u;
        unsigned color=(pattern&1u)?0xf800:0xffff;
        set_palette_color(palette,1,color);
        source[0]=(uint8_t)(pattern>=2u?2u:1u);
        unsigned id=9999;int rc=msi_gpu_snapshot_indexed8(&multi_cache,&state,source,4,palette,512,&id);
        NEED(rc==MSI_GPU_SNAPSHOT_HIT||rc==MSI_GPU_SNAPSHOT_NEW);
        if(rc==MSI_GPU_SNAPSHOT_NEW)misses++;else hits++;
        ids[i]=id;expected[i]=pattern>=2u?0x001f:color;
        float x=(float)((i%8u)*80u+8u),y=(float)((i/8u)*80u+8u);
        verts[3*i].xy[0]=x;verts[3*i].xy[1]=y;
        verts[3*i+1].xy[0]=x+64;verts[3*i+1].xy[1]=y;
        verts[3*i+2].xy[0]=x;verts[3*i+2].xy[1]=y+64;
        verts[3*i].uv[0]=0;verts[3*i].uv[1]=0;
        verts[3*i+1].uv[0]=1;verts[3*i+1].uv[1]=0;
        verts[3*i+2].uv[0]=0;verts[3*i+2].uv[1]=1;
    }
    NEED(multi_cache.count==4&&hits==12&&misses==4);
    /* Destroy the caller's current contents before any GPU access. */
    memset(source,0,sizeof(source));memset(palette,0,sizeof(palette));
    Image textures[MULTI_N]={0};
    VkDescriptorSet sets[MULTI_N]={0};VkDescriptorSetLayout layouts[MULTI_N];
    VkDescriptorPool pool;VkDescriptorPoolSize ps={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2u*multi_cache.count};
    VkDescriptorPoolCreateInfo pi=S(DESCRIPTOR_POOL_CREATE_INFO);pi.maxSets=multi_cache.count;pi.poolSizeCount=1;pi.pPoolSizes=&ps;
    OK(vkCreateDescriptorPool(dev,&pi,0,&pool));
    for(unsigned i=0;i<multi_cache.count;i++)layouts[i]=dsl;
    VkDescriptorSetAllocateInfo da=S(DESCRIPTOR_SET_ALLOCATE_INFO);da.descriptorPool=pool;da.descriptorSetCount=multi_cache.count;da.pSetLayouts=layouts;
    OK(vkAllocateDescriptorSets(dev,&da,sets));
    Buffer stage=buffer(multi_cache.decoded_used*4u,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    writebuf(stage,multi_cache.decoded,multi_cache.decoded_used*4u);
    for(unsigned i=0;i<multi_cache.count;i++){
        textures[i]=image(2,2,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        VkDescriptorImageInfo ti[2]={{sampler,textures[i].v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler,tex.v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
        VkWriteDescriptorSet wr[2]={S(WRITE_DESCRIPTOR_SET),S(WRITE_DESCRIPTOR_SET)};
        for(unsigned k=0;k<2;k++){
            wr[k].dstSet=sets[i];wr[k].dstBinding=k;wr[k].descriptorCount=1;
            wr[k].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;wr[k].pImageInfo=&ti[k];
        }
        vkUpdateDescriptorSets(dev,2,wr,0,0);
    }
    Buffer vertices=buffer(sizeof(verts),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);writebuf(vertices,verts,sizeof(verts));
    Image out=image(w,h,VK_FORMAT_R16_UINT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    Buffer read=buffer((VkDeviceSize)w*h*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkFramebuffer fb;VkFramebufferCreateInfo fi=S(FRAMEBUFFER_CREATE_INFO);fi.renderPass=rp;fi.attachmentCount=1;fi.pAttachments=&out.v;fi.width=w;fi.height=h;fi.layers=1;
    OK(vkCreateFramebuffer(dev,&fi,0,&fb));
    VkCommandBuffer cmd=begin();
    for(unsigned i=0;i<multi_cache.count;i++){
        transition(cmd,textures[i].i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy br={0};br.bufferOffset=multi_cache.entries[i].decoded_offset*4u;
        br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};br.imageExtent=(VkExtent3D){2,2,1};
        vkCmdCopyBufferToImage(cmd,stage.b,textures[i].i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&br);
        transition(cmd,textures[i].i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    }
    VkClearValue clear={.color.uint32={0xdead,0,0,0}};
    VkRenderPassBeginInfo bi=S(RENDER_PASS_BEGIN_INFO);bi.renderPass=rp;bi.framebuffer=fb;bi.renderArea.extent=(VkExtent2D){w,h};bi.clearValueCount=1;bi.pClearValues=&clear;
    vkCmdBeginRenderPass(cmd,&bi,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp={0,0,(float)w,(float)h,0,1};VkRect2D sc={{0,0},{w,h}};
    vkCmdSetViewport(cmd,0,1,&vp);vkCmdSetScissor(cmd,0,1,&sc);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipe);
    VkDeviceSize off=0;vkCmdBindVertexBuffers(cmd,0,1,&vertices.b,&off);
    Push push={{640,400},{(float)w,(float)h},70u,0};
    vkCmdPushConstants(cmd,pl,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
    for(unsigned i=0;i<MULTI_N;i++){
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pl,0,1,&sets[ids[i]],0,0);
        vkCmdDraw(cmd,3,1,i*3u,0);
    }
    vkCmdEndRenderPass(cmd);
    transition(cmd,out.i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy br={0};br.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};br.imageExtent=(VkExtent3D){w,h,1};
    vkCmdCopyImageToBuffer(cmd,out.i,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read.b,1,&br);
    VkBufferMemoryBarrier bar=S(BUFFER_MEMORY_BARRIER);bar.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;bar.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    bar.srcQueueFamilyIndex=bar.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bar.buffer=read.b;bar.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,0,1,&bar,0,0);end(cmd);
    void *mapped;OK(vkMapMemory(dev,read.m,0,read.size,0,&mapped));const uint16_t *p=mapped;
    unsigned checked=0,errors=0,keyed=0,visible=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        double lx=(x+.5)*640/w,ly=(y+.5)*400/h;
        unsigned tile=(unsigned)(ly/80)*8u+(unsigned)(lx/80);
        double a=lx-(double)((tile%8u)*80u+8u),b=ly-(double)((tile/8u)*80u+8u);
        double m=fmin(a,fmin(b,64-a-b));
        uint16_t want=0xdead;
        if(tile<MULTI_N){
            if(fabs(m)<.1||fabs(a-32)<.1||fabs(b-32)<.1)continue;
            if(m>0){
                if(a>=32&&b<32)keyed++;
                else {want=(uint16_t)(b>=32?((tile&1u)?0xf800:0xffff):expected[tile]);visible++;}
            }
        }
        checked++;if(p[(size_t)y*w+x]!=want){if(errors<3)fprintf(stderr,"indexed batch %u,%u expected=%04x got=%04x\n",x,y,want,p[(size_t)y*w+x]);errors++;}
    }
    printf("VULKAN_3D_INDEXED_BATCH %ux%u draws=%u unique=%u hits=%u submissions=1 checked=%u keyed=%u visible=%u errors=%u\n",
        w,h,MULTI_N,misses,hits,checked,keyed,visible,errors);
    vkUnmapMemory(dev,read.m);vkDestroyFramebuffer(dev,fb,0);vkDestroyDescriptorPool(dev,pool,0);
    for(unsigned i=0;i<multi_cache.count;i++)freeimage(textures[i]);
    freeimage(out);freebuf(stage);freebuf(vertices);freebuf(read);msi_gpu_snapshot_reset(&multi_cache);
    return checked>10000&&keyed>1000&&visible>1000&&errors==0;
}

int main(void){
#ifdef _WIN32
    if(!freopen("Vulkan3DProbe.log","w",stdout))return 1;
    if(_dup2(_fileno(stdout),_fileno(stderr))!=0)return 1;
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    puts("MatroxVulkan3DProbe x86 v9 / embedded world shaders with key metadata");
    if(!load_world_vk())return 1;
#endif
    oracle_selftest();
    setup();
    const unsigned modes[]={0,1,7,9,15,25,63,64,73,79,127};
    unsigned failures=0;
    for(unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);i++){failures+=!test(640,400,modes[i]);failures+=!test(1440,1080,modes[i]);}
    /* Real source encodings: decoded upload versus independently specified
       oracle values. 4bpp is intentionally nearest only, as in the CPU path. */
    uint8_t lut[512]={0};
    lut[6]=0;lut[7]=0xf8;lut[0]=0xe0;lut[1]=7;lut[8]=0x1f;lut[10]=0xff;lut[11]=0xff;
    const uint8_t indexed8[]={3,0,4,5};
    const uint8_t indexed4[]={0x03,0x54};
    const uint8_t direct16[]={0,0xf8,0xe0,7,0x1f,0,0xff,0xff};
    const uint32_t expected[]={0xf800,0x307e0,0x001f,0xffff};
    MsiGpuTexture fixture={2,2,8,1,0,0xffff,1,0,0};
    replace_fixture(&fixture,indexed8,sizeof(indexed8),lut,sizeof(lut),expected);
    const unsigned decoded_modes[]={64,73,79,127};
    for(unsigned i=0;i<4;i++){failures+=!test(640,400,decoded_modes[i]);failures+=!test(1440,1080,decoded_modes[i]);}
    fixture.planes=4;fixture.key=0x07e0;
    replace_fixture(&fixture,indexed4,sizeof(indexed4),lut,32,expected);
    failures+=!test(640,400,64);failures+=!test(1440,1080,64);
    fixture.planes=16;
    replace_fixture(&fixture,direct16,sizeof(direct16),NULL,0,expected);
    for(unsigned i=0;i<4;i++){failures+=!test(640,400,decoded_modes[i]);failures+=!test(1440,1080,decoded_modes[i]);}
    const unsigned opacities[]={0,25,50,100};
    for(unsigned i=0;i<4;i++){failures+=!test_case(640,400,127,(int)opacities[i],0);failures+=!test_case(1440,1080,127,(int)opacities[i],0);}
    for(unsigned mode=1;mode<=2;mode++){failures+=!test_case(640,400,127,50,mode);failures+=!test_case(1440,1080,127,50,mode);}
    failures+=!test_batch(640,400);failures+=!test_batch(1440,1080);
    failures+=!test_indexed_batch(640,400);failures+=!test_indexed_batch(1440,1080);
    vkDestroyPipeline(dev,pipe,0);vkDestroyRenderPass(dev,rp_load,0);vkDestroyRenderPass(dev,rp,0);vkDestroyDescriptorPool(dev,dp,0);vkDestroyPipelineLayout(dev,pl,0);vkDestroyDescriptorSetLayout(dev,dsl,0);
    vkDestroySampler(dev,sampler,0);freeimage(tex);freebuf(vb);freebuf(backdrop_vb);freebuf(upload);vkDestroyCommandPool(dev,cp,0);vkDestroyDevice(dev,0);vkDestroyInstance(inst,0);
    if(failures)printf("VULKAN_3D_OFFSCREEN_FAIL cases_failed=%u\n",failures);
    else puts("VULKAN_3D_OFFSCREEN_PASS cases=56");
#ifdef _WIN32
    FreeLibrary(world_vk_loader);
#endif
    return failures?1:0;
}
