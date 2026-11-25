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
#include "msi_gpu_draw_batch.h"
#include "msi_gpu_compare.h"
#include "msi_gpu_bypass.h"
#ifdef _WIN32
#include "world_vk_dispatch.h"
#endif
#include "msi_vk_batch_replay.h"
#include "msi_vk_shadow_session.h"
#include "msi_vk_shadow_bridge.h"
static MsiVkBatchContext batch_context;
#define OK(x) do { VkResult r_=(x); if(r_!=VK_SUCCESS){fprintf(stderr,"%s: %d\n",#x,r_);exit(1);} }while(0)
#define NEED(x) do {if(!(x)){fprintf(stderr,"check failed: %s\n",#x);exit(1);}}while(0)
#define S(t) {.sType=VK_STRUCTURE_TYPE_##t}
typedef MsiGpuVertex Vertex;
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
    batch_context=(MsiVkBatchContext){.phys=phys,.dev=dev,.queue=queue,.cp=cp,.mem=mem,.dummy=tex.v};
    OK(msi_vk_batch_pipeline_create(&batch_context,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),0));
    sampler=batch_context.sampler;dsl=batch_context.dsl;pl=batch_context.pl;rp=batch_context.rp;rp_load=batch_context.rp_load;pipe=batch_context.pipe;
    VkDescriptorPoolSize ps={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2};
    VkDescriptorPoolCreateInfo pd=S(DESCRIPTOR_POOL_CREATE_INFO);pd.maxSets=1;pd.poolSizeCount=1;pd.pPoolSizes=&ps;OK(vkCreateDescriptorPool(dev,&pd,0,&dp));
    VkDescriptorSetAllocateInfo da=S(DESCRIPTOR_SET_ALLOCATE_INFO);da.descriptorPool=dp;da.descriptorSetCount=1;da.pSetLayouts=&dsl;OK(vkAllocateDescriptorSets(dev,&da,&ds));
    VkDescriptorImageInfo ti={sampler,tex.v,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr=S(WRITE_DESCRIPTOR_SET);wr.dstSet=ds;wr.descriptorCount=1;wr.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;wr.pImageInfo=&ti;vkUpdateDescriptorSets(dev,1,&wr,0,0);
    wr.dstBinding=1;vkUpdateDescriptorSets(dev,1,&wr,0,0);
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
static MsiGpuDrawBatch multi_batch;
#define multi_cache (multi_batch.snapshots)
static void set_palette_color(uint8_t *p,unsigned idx,unsigned color){
    p[2u*idx]=(uint8_t)color;p[2u*idx+1u]=(uint8_t)(color>>8);
}
static uint16_t *render_recorded(const MsiGpuDrawBatch *batch,unsigned w,unsigned h,const uint16_t *before){
    MsiVkBatchWork work={0};
    uint16_t *result=malloc((size_t)w*h*2);NEED(result);
    OK(msi_vk_batch_render(&batch_context,&work,batch,w,h,before,result,0));
    return result;
}
static void test_pipeline_recovery(void){
    MsiVkBatchContext c={.phys=phys,.dev=dev,.queue=queue,.cp=cp,.mem=mem,.dummy=tex.v};
    for(unsigned fault=1;fault<=8;fault++){
        NEED(msi_vk_batch_pipeline_create(&c,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),fault)==VK_ERROR_OUT_OF_HOST_MEMORY);
        NEED(!c.pipe&&!c.rp_load&&!c.rp&&!c.pl&&!c.dsl&&!c.sampler&&c.dev==dev);
    }
    OK(msi_vk_batch_pipeline_create(&c,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),0));
    NEED(c.pipe&&c.rp_load&&c.rp&&c.pl&&c.dsl&&c.sampler);
    msi_vk_batch_pipeline_destroy(&c);msi_vk_batch_pipeline_destroy(&c);
    NEED(!c.pipe&&!c.rp_load&&!c.rp&&!c.pl&&!c.dsl&&!c.sampler&&c.dev==dev);
    puts("VULKAN_3D_PIPELINE_RECOVERY_PASS injected=8");
}
static void test_batch_recovery(const MsiGpuDrawBatch *batch){
    MsiVkBatchContext c=batch_context; /* test-only borrowed copy, no destruction */
    MsiVkBatchWork work={0};uint16_t result[64];
    for(unsigned fault=1;fault<=6;fault++){
        for(unsigned i=0;i<64;i++)result[i]=0xbeef;
        NEED(msi_vk_batch_render(&c,&work,batch,8,8,NULL,result,fault)==VK_ERROR_OUT_OF_HOST_MEMORY);
        NEED(!work.pending&&!work.pool&&!work.cmd&&!work.fb&&!work.out.i&&!work.stage.b);
        for(unsigned i=0;i<64;i++)NEED(result[i]==0xbeef);
    }
    OK(msi_vk_batch_render(&c,&work,batch,8,8,NULL,result,0));
    NEED(!work.pending&&!work.pool&&!work.cmd);
    puts("VULKAN_3D_BATCH_RECOVERY_PASS injected=6");
    for(unsigned i=0;i<64;i++)result[i]=0xbeef;
    NEED(msi_vk_batch_render(&c,&work,batch,8,8,NULL,result,7)==VK_ERROR_OUT_OF_HOST_MEMORY);
    NEED(c.unusable&&work.pending&&work.pool&&work.cmd);
    NEED(!mvk_cleanup(&c,&work));
    NEED(msi_vk_batch_render(&c,&work,batch,8,8,NULL,result,0)==VK_ERROR_INITIALIZATION_FAILED);
    for(unsigned i=0;i<64;i++)NEED(result[i]==0xbeef);
    OK(msi_vk_batch_retire(&c,&work));NEED(!work.pending&&!work.pool&&c.unusable);
    /* Both injected wait errors follow actual successful idle: the real
     * device is healthy; use a fresh test-only context copy for lost case. */
    c=batch_context;
    NEED(msi_vk_batch_render(&c,&work,batch,8,8,NULL,result,8)==VK_ERROR_DEVICE_LOST);
    NEED(c.unusable&&!work.pending&&!work.pool&&!work.cmd);
    NEED(msi_vk_batch_render(&c,&work,batch,8,8,NULL,result,0)==VK_ERROR_INITIALIZATION_FAILED);
    for(unsigned i=0;i<64;i++)NEED(result[i]==0xbeef);
    puts("VULKAN_3D_BATCH_WAIT_RECOVERY_PASS simulated=2");
}

/* Compare the new tagged R32 attachment against the already validated
 * two-background R16 replay, including keyed discards and same-color writes. */
static void test_tagged_output(const MsiGpuDrawBatch *batch){
    MsiVkBatchContext tagged={.phys=phys,.dev=dev,.queue=queue,.cp=cp,.mem=mem,.tagged_output=1};
    MsiVkBatchWork work={0};const size_t n=640u*400u;
    uint16_t *before=malloc(n*2u),*a=malloc(n*2u),*b=malloc(n*2u);
    uint32_t *result=malloc(n*4u);NEED(before&&a&&b&&result);
    for(size_t i=0;i<n;i++)before[i]=0xdead;
    OK(msi_vk_batch_render(&batch_context,&work,batch,640,400,before,a,0));
    for(size_t i=0;i<n;i++)before[i]=(uint16_t)(0xdeadu^65535u);
    OK(msi_vk_batch_render(&batch_context,&work,batch,640,400,before,b,0));
    OK(msi_vk_batch_pipeline_create(&tagged,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),0));
    for(size_t i=0;i<n;i++)result[i]=0xabcdef01u;
    NEED(msi_vk_batch_render(&tagged,&work,batch,640,400,before,result,0)==VK_ERROR_INITIALIZATION_FAILED);
    NEED(msi_vk_batch_render(&tagged,&work,batch,640,400,NULL,result,6)==VK_ERROR_OUT_OF_HOST_MEMORY);
    for(size_t i=0;i<n;i++)NEED(result[i]==0xabcdef01u);
    OK(msi_vk_batch_render(&tagged,&work,batch,640,400,NULL,result,0));
    unsigned written=0,untouched=0;
    for(size_t i=0;i<n;i++){
        if(a[i]==b[i]){NEED(result[i]==a[i]);written++;}
        else {NEED(a[i]==0xdead&&b[i]==before[i]&&result[i]==65536u);untouched++;}
    }
    NEED(written&&untouched);
    msi_vk_batch_pipeline_destroy(&tagged);
    free(before);free(a);free(b);free(result);
    printf("VULKAN_3D_TAGGED_OUTPUT_PASS written=%u untouched=%u\n",written,untouched);
}
/* Exercise real ordered overlapping draws with immutable snapshots.
 * Compare one tagged batch against sequential tagged single submissions.
 * All triangles share keyed provenance, but textures and colors differ. */
static void test_tagged_batch_commit(const MsiGpuDrawBatch *batch){
    NEED(batch->count>=4u);
    MsiGpuDrawBatch *ordered=malloc(sizeof(*ordered)),*one=malloc(sizeof(*one));
    NEED(ordered&&one);*ordered=*batch;ordered->count=4;
    for(unsigned i=0;i<4u;i++){
        ordered->draws[i].flags=79u;
        ordered->draws[i].bounds=(MsiGpuRect){8u+8u*i,8,72u+8u*i,72};
        for(unsigned j=0;j<3u;j++){
            ordered->vertices[3u*i+j]=batch->vertices[j];
            ordered->vertices[3u*i+j].xy[0]+=8.f*i;
            ordered->vertices[3u*i+j].iw=1.f;
        }
    }
    MsiVkBatchContext tagged={.phys=phys,.dev=dev,.queue=queue,.cp=cp,.mem=mem,.tagged_output=1};
    MsiVkBatchWork work={0};const size_t n=640u*400u;
    uint16_t *before=malloc(n*2u),*live=malloc(n*2u),*reference=malloc(n*2u);
    uint8_t *under=malloc(n),*ref_under=malloc(n);
    uint32_t *out=malloc(n*4u);NEED(before&&live&&reference&&under&&ref_under&&out);
    for(size_t j=0;j<n;j++){before[j]=live[j]=reference[j]=0xdead;under[j]=ref_under[j]=3;}
    OK(msi_vk_batch_pipeline_create(&tagged,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),0));
    *one=*ordered;one->count=1;
    unsigned overwritten=0;
    for(unsigned i=0;i<ordered->count;i++){
        one->draws[0]=ordered->draws[i];
        for(unsigned j=0;j<3u;j++)one->vertices[j]=ordered->vertices[3u*i+j];
        OK(msi_vk_batch_render(&tagged,&work,one,640,400,NULL,out,0));
        for(size_t j=0;j<n;j++){
            if(out[j]==65536u)continue;
            NEED(out[j]<=65535u);
            overwritten+=ref_under[j]==2;
            reference[j]=(uint16_t)out[j];ref_under[j]=2;
        }
    }
    NEED(overwritten>0);
    OK(msi_vk_batch_render(&tagged,&work,ordered,640,400,NULL,out,0));
    MsiGpuBypass result;
    NEED(msi_gpu_bypass_tagged_batch_commit(&result,640,400,ordered,live,under,before,out));
    NEED(result.written>0&&result.changed>0);
    for(size_t j=0;j<n;j++)NEED(live[j]==reference[j]&&under[j]==ref_under[j]);
    printf("VULKAN_3D_TAGGED_BATCH_COMMIT_PASS draws=4 written=%u overwritten=%u\n",result.written,overwritten);
    msi_vk_batch_pipeline_destroy(&tagged);
    free(out);free(ref_under);free(under);free(reference);free(live);free(before);free(one);free(ordered);
}
static void test_shadow_session(const MsiGpuDrawBatch *batch,const uint16_t *expected){
    MsiVkShadowSession session={0};
    const size_t n=640u*400u;
    uint16_t *before=malloc(n*2u),*scratch=malloc(n*2u);NEED(before&&scratch);
    for(size_t i=0;i<n;i++)before[i]=0xdead;
    MsiGpuCompare stats={0};
    for(unsigned fault=1;fault<=9;fault++){
        NEED(msi_vk_shadow_open(&session,phys,dev,queue,qfamily,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),fault)==VK_ERROR_OUT_OF_HOST_MEMORY);
        NEED(!session.ready&&!session.ctx.dev&&!session.ctx.cp&&!session.work.pending);
    }
    OK(msi_vk_shadow_open(&session,phys,dev,queue,qfamily,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),0));
    NEED(session.ready&&session.ctx.cp!=cp);
    NEED(msi_vk_shadow_open(&session,phys,dev,queue,qfamily,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),0)==VK_ERROR_INITIALIZATION_FAILED);
    OK(msi_vk_shadow_compare(&session,batch,before,expected,scratch,&stats,0));
    NEED(stats.exact==n&&msi_gpu_compare_pass(&stats,(unsigned)n));
    /* Repeated submissions and a failure must leave CPU inputs untouched. */
    for(size_t i=0;i<n;i++)scratch[i]=0xbeef;
    stats.exact=123u;
    NEED(msi_vk_shadow_compare(&session,batch,before,expected,scratch,&stats,7)==VK_ERROR_OUT_OF_HOST_MEMORY);
    NEED(!session.ready&&session.work.pending&&stats.exact==123u);
    for(size_t i=0;i<n;i++)NEED(before[i]==0xdead&&scratch[i]==0xbeef);
    OK(msi_vk_shadow_close(&session));OK(msi_vk_shadow_close(&session));
    NEED(!session.ctx.dev&&!session.work.pending);
    OK(msi_vk_shadow_open(&session,phys,dev,queue,qfamily,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),0));
    OK(msi_vk_shadow_compare(&session,batch,before,expected,scratch,&stats,0));
    NEED(stats.exact==n&&msi_gpu_compare_pass(&stats,(unsigned)n));
    OK(msi_vk_shadow_close(&session));free(before);free(scratch);
    puts("VULKAN_3D_SHADOW_SESSION_PASS open_failures=9 reopen=1");
}
static PFN_vkGetInstanceProcAddr bridge_test_gipa;
static PFN_vkGetDeviceProcAddr bridge_test_gdpa;
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL bridge_missing_device_command(VkDevice d,const char *name){
    if(strcmp(name,"vkCreateGraphicsPipelines")==0)return NULL;
    return bridge_test_gdpa(d,name);
}
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL bridge_missing_instance_command(VkInstance i,const char *name){
    if(strcmp(name,"vkGetDeviceProcAddr")==0)return (PFN_vkVoidFunction)bridge_missing_device_command;
    return bridge_test_gipa(i,name);
}
static void test_shadow_bridge(const MsiGpuDrawBatch *batch,const uint16_t *expected){
#ifdef _WIN32
    bridge_test_gipa=(PFN_vkGetInstanceProcAddr)GetProcAddress(world_vk_loader,"vkGetInstanceProcAddr");
#else
    bridge_test_gipa=vkGetInstanceProcAddr;
#endif
    NEED(bridge_test_gipa);
    bridge_test_gdpa=(PFN_vkGetDeviceProcAddr)bridge_test_gipa(inst,"vkGetDeviceProcAddr");
    NEED(bridge_test_gdpa);
    const size_t n=640u*400u;
    uint16_t *before=malloc(n*2u),*scratch=malloc(n*2u);NEED(before&&scratch);
    for(size_t i=0;i<n;i++){before[i]=0xdead;scratch[i]=0xbeef;}
    MsiGpuCompare stats={0};
    NEED(msi_vk_bridge_open(inst,phys,dev,queue,qfamily,bridge_missing_instance_command,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag))==VK_ERROR_INITIALIZATION_FAILED);
    NEED(msi_vk_bridge_compare(batch,before,expected,scratch,&stats)==VK_ERROR_INITIALIZATION_FAILED);
    for(size_t i=0;i<n;i++)NEED(scratch[i]==0xbeef);
    OK(msi_vk_bridge_close());
    for(unsigned run=0;run<2;run++){
        OK(msi_vk_bridge_open(inst,phys,dev,queue,qfamily,bridge_test_gipa,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag)));
        NEED(msi_vk_bridge_open(inst,phys,dev,queue,qfamily,bridge_test_gipa,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag))==VK_ERROR_INITIALIZATION_FAILED);
        OK(msi_vk_bridge_compare(batch,before,expected,scratch,&stats));
        NEED(stats.exact==n&&msi_gpu_compare_pass(&stats,(unsigned)n));
        uint32_t *tagged=malloc(n*4u);NEED(tagged);
        OK(msi_vk_bridge_tagged(batch,tagged,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag)));
        unsigned written=0;
        for(size_t i=0;i<n;i++){
            NEED(tagged[i]<=65536u);
            NEED((tagged[i]==65536u?before[i]:tagged[i])==expected[i]);
            written+=tagged[i]!=65536u;
        }
        NEED(written);
        free(tagged);
        /* R16 and R32 share a queue/pool, not pipeline objects. */
        OK(msi_vk_bridge_compare(batch,before,expected,scratch,&stats));
        NEED(stats.exact==n&&msi_gpu_compare_pass(&stats,(unsigned)n));
        OK(msi_vk_bridge_close());OK(msi_vk_bridge_close());
    }
    free(before);free(scratch);
    puts("VULKAN_3D_TAGGED_BRIDGE_PASS reopen=2");
    puts("VULKAN_3D_SHADOW_BRIDGE_PASS missing_dispatch=1 reopen=1");
}
static int test_indexed_batch(unsigned w,unsigned h){
    MsiGpuTexture state={2,2,8,1,0,0xffff,1,0,0};
    uint8_t source[4]={1,0,1,1},palette[512]={0};
    multi_cache.raw=multi_raw;multi_cache.raw_capacity=sizeof(multi_raw);
    multi_cache.decoded=multi_decoded;multi_cache.decoded_capacity=sizeof(multi_decoded)/sizeof(multi_decoded[0]);
    msi_gpu_draw_batch_reset(&multi_batch);
    /* Keyed green is never drawn. Blue remains available for direct writes. */
    set_palette_color(palette,0,0x07e0);
    set_palette_color(palette,2,0x001f);
    Vertex verts[MULTI_N*3u]={0};
    unsigned expected[MULTI_N];
    for(unsigned i=0;i<MULTI_N;i++){
        unsigned pattern=i%4u;
        unsigned color=(pattern&1u)?0xf800:0xffff;
        set_palette_color(palette,1,color);
        source[0]=(uint8_t)(pattern>=2u?2u:1u);
        expected[i]=pattern>=2u?0x001f:color;
        float x=(float)((i%8u)*80u+8u),y=(float)((i/8u)*80u+8u);
        verts[3*i].xy[0]=x;verts[3*i].xy[1]=y;
        verts[3*i+1].xy[0]=x+64;verts[3*i+1].xy[1]=y;
        verts[3*i+2].xy[0]=x;verts[3*i+2].xy[1]=y+64;
        verts[3*i].uv[0]=0;verts[3*i].uv[1]=0;
        verts[3*i+1].uv[0]=1;verts[3*i+1].uv[1]=0;
        verts[3*i+2].uv[0]=0;verts[3*i+2].uv[1]=1;
        MsiGpuRect bounds={(unsigned)x*w/640u,(unsigned)y*h/400u,
            ((unsigned)(x+64)*w+639u)/640u,((unsigned)(y+64)*h+399u)/400u};
        NEED(msi_gpu_draw_batch_push(&multi_batch,&verts[3*i],bounds,70u,&state,source,4,palette,512)==MSI_GPU_DRAW_ACCEPT);
    }
    unsigned hits=multi_batch.hits,misses=multi_batch.misses;
    NEED(multi_batch.count==MULTI_N&&multi_cache.count==4&&hits==12&&misses==4);
    memset(verts,0,sizeof(verts));
    /* Destroy the caller's current contents before any GPU access. */
    memset(source,0,sizeof(source));memset(palette,0,sizeof(palette));
    if(w==640u&&h==400u)test_batch_recovery(&multi_batch);
    uint16_t *p=render_recorded(&multi_batch,w,h,NULL);
    if(w==640u&&h==400u){test_shadow_session(&multi_batch,p);test_shadow_bridge(&multi_batch,p);test_tagged_output(&multi_batch);test_tagged_batch_commit(&multi_batch);}
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
    free(p);msi_gpu_draw_batch_reset(&multi_batch);
    return checked>10000&&keyed>1000&&visible>1000&&errors==0;
}


/* Local capture reader. Exact file size and all bounds checked before GPU
 * allocation; captured content is never uploaded to any network service. */
#define CAPTURE_PIXELS (640u*400u)
static MsiGpuDrawBatch replay_batch;
static uint16_t replay_before[CAPTURE_PIXELS],replay_after[CAPTURE_PIXELS];
static unsigned replay_frame;
static int read_capture(const char *path){
    /* Every input starts with empty ownership and counts, including after a rejected file. */
    msi_gpu_draw_batch_reset(&replay_batch);
    FILE *f=fopen(path,"rb");if(!f){fprintf(stderr,"capture open failed\n");return 0;}
    uint8_t magic[8];uint32_t hdr[6],rec[3];unsigned ok=0;
    MsiGpuDrawBatch *b=&replay_batch;
    if(fread(magic,1,8,f)!=8||memcmp(magic,"M95GPU01",8)||fread(hdr,4,6,f)!=6)goto done;
    const unsigned n=hdr[2],nt=hdr[3],words=hdr[4];
    if(hdr[0]!=640||hdr[1]!=400||n<8||n>32||!nt||nt>n||!words||words>2u*1024u*1024u)goto done;
    if(fseek(f,0,SEEK_END)!=0)goto done;
    long size=ftell(f);
    size_t expected=32u+12u*nt+120u*n+4u*words+4u*CAPTURE_PIXELS;
    if(size<0||(size_t)size!=expected||fseek(f,32,SEEK_SET)!=0)goto done;
    unsigned offset=0;
    for(unsigned i=0;i<nt;i++){
        if(fread(rec,4,3,f)!=3||!rec[0]||!rec[1]||rec[0]>2048u||rec[1]>2048u||rec[2]!=offset)goto done;
        unsigned count=rec[0]*rec[1];
        if(count>1024u*1024u||count>words-offset)goto done;
        b->snapshots.entries[i].state.width=rec[0];b->snapshots.entries[i].state.height=rec[1];
        b->snapshots.entries[i].decoded_offset=offset;b->snapshots.entries[i].decoded_count=count;
        offset+=count;
    }
    if(offset!=words||fread(b->draws,sizeof(MsiGpuDraw),n,f)!=n||fread(b->vertices,sizeof(Vertex),3u*n,f)!=3u*n)goto done;
    for(unsigned i=0;i<n;i++){
        const MsiGpuDraw *d=&b->draws[i];const MsiGpuRect q=d->bounds;
        if(d->texture_id>=nt||(d->flags&~127u)||q.x0>=q.x1||q.y0>=q.y1||q.x1>640||q.y1>400)goto done;
        for(unsigned j=0;j<3;j++){
            const Vertex *v=&b->vertices[i*3u+j];
            if(!msi_gpu_vertex_valid(v,d->flags)||(!(d->flags&1u)&&v->iw!=1.f))goto done;
            if(!(d->flags&16u)&&(v->m[0]!=255.f||v->m[1]!=255.f||v->m[2]!=255.f))goto done;
        }
    }
    b->snapshots.decoded=malloc((size_t)words*4);if(!b->snapshots.decoded)goto done;
    if(fread(b->snapshots.decoded,4,words,f)!=words||
       fread(replay_before,2,CAPTURE_PIXELS,f)!=CAPTURE_PIXELS||
       fread(replay_after,2,CAPTURE_PIXELS,f)!=CAPTURE_PIXELS)goto done;
    for(unsigned i=0;i<words;i++)if(b->snapshots.decoded[i]&~0x3ffffu)goto done;
    b->snapshots.count=nt;b->snapshots.decoded_used=words;b->count=n;replay_frame=hdr[5];ok=1;
done:
    fclose(f);
    if(!ok){free(b->snapshots.decoded);b->snapshots.decoded=NULL;fprintf(stderr,"invalid M95GPU01 capture\n");}
    return ok;
}
static int replay_capture(void){
    MsiGpuDrawBatch *b=&replay_batch;
    uint16_t *p=render_recorded(b,640,400,replay_before);
    MsiGpuCompare r;
    msi_gpu_compare(&r,b,640,400,replay_before,replay_after,p);
    for(unsigned j=0;j<r.bad&&j<MSI_GPU_COMPARE_EXAMPLES;j++){
        const MsiGpuCompareExample *e=&r.examples[j];
        fprintf(stderr,"replay mismatch %u,%u expected=%04x got=%04x\n",e->x,e->y,e->expected,e->actual);
    }
    printf("VULKAN_3D_REPLAY frame=%u draws=%u textures=%u exact=%u one_lsb=%u edge=%u unexplained=%u outside_bounds=%u cpu_changed=%u gpu_changed=%u\n",
        replay_frame,b->count,b->snapshots.count,r.exact,r.one,r.edge,r.bad,r.outside,r.cpu_changed,r.gpu_changed);
    printf("VULKAN_3D_REPLAY_ACTIVE union=%u exact=%u one_lsb=%u edge=%u unexplained=%u outside_bounds=%u cpu_only=%u gpu_only=%u both_changed=%u\n",
        r.active,r.active_exact,r.one,r.edge,r.bad,r.outside,r.cpu_only,r.gpu_only,r.both_changed);
    printf("VULKAN_3D_REPLAY_FLAGS perspective=%u bilinear=%u affine_nearest=%u modulated=%u dithered=%u keyed=%u\n",
        r.perspective,r.bilinear,r.affine_nearest,r.modulated,r.dithered,r.keyed);
    int counts_ok=msi_gpu_compare_counts_ok(&r,CAPTURE_PIXELS);
    if(!counts_ok)fprintf(stderr,"inconsistent replay pixel accounting\n");
    free(p);free(b->snapshots.decoded);b->snapshots.decoded=NULL;
    /* A diagnostic, not a bit-exact certification. Report all categories.
       No arbitrary acceptance of large errors away from geometric edges. */
    return msi_gpu_compare_pass(&r,CAPTURE_PIXELS);
}

int main(int argc,char **argv){
#ifdef _WIN32
    if(!freopen("Vulkan3DProbe.log","w",stdout))return 1;
    if(_dup2(_fileno(stdout),_fileno(stderr))!=0)return 1;
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    puts("MatroxVulkan3DProbe x86 v13 / multi-capture replay diagnostics");
    if(!load_world_vk())return 1;
#endif
    if(argc>65){fprintf(stderr,"usage: MatroxVulkan3DProbe [capture.bin ...] (up to 64)\n");return 2;}
    oracle_selftest();
    unsigned failures=0,invalid=0,passed=0;
    int ready=0;
    if(argc>1){
        for(int i=1;i<argc;i++){
            printf("VULKAN_3D_REPLAY_INPUT index=%d file=%s\n",i,argv[i]);
            if(!read_capture(argv[i])){
                invalid++;puts("VULKAN_3D_REPLAY_INVALID");continue;
            }
            if(!ready){setup();ready=1;}
            if(replay_capture()){passed++;puts("VULKAN_3D_REPLAY_PASS");}
            else {failures++;puts("VULKAN_3D_REPLAY_DIFFERENCES");}
        }
        printf("VULKAN_3D_REPLAY_SUMMARY total=%d passed=%u differences=%u invalid=%u\n",argc-1,passed,failures,invalid);
    }else {
    setup();ready=1;test_pipeline_recovery();
    const unsigned modes[]={0,1,7,9,15,25,63,64,73,79,127};
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
    }
    if(ready){
    vkDestroyDescriptorPool(dev,dp,0);msi_vk_batch_pipeline_destroy(&batch_context);
    freeimage(tex);freebuf(vb);freebuf(backdrop_vb);freebuf(upload);vkDestroyCommandPool(dev,cp,0);vkDestroyDevice(dev,0);vkDestroyInstance(inst,0);
    }
    if(argc==1){
        if(failures)printf("VULKAN_3D_OFFSCREEN_FAIL cases_failed=%u\n",failures);
        else puts("VULKAN_3D_OFFSCREEN_PASS cases=56");
    }
#ifdef _WIN32
    FreeLibrary(world_vk_loader);
#endif
    return invalid?2:(failures?1:0);
}
