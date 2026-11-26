/* SPDX-License-Identifier: GPL-3.0-or-later
 * Reusable native Win32 x86 Vulkan presenter; independent of MSI95 ABI.
 * No window creation, message pump or application-owned framebuffer writes.
 */
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>
#include <string.h>
#include "vulkan_present.h"
#ifdef MATROX_VK_SHADOW
#include "msi_vk_shadow_bridge.h"
#include "world_spv_embedded.h"
#endif
/* The MSI95 object is linked without CRT. Keep large scratch buffers in
 * static storage (one owning thread) and supply compiler-generated memory
 * intrinsics locally, not a new redistributable dependency. */
#ifdef MATROX_VK_FREESTANDING
void *memset(void *p,int c,size_t n){
    volatile unsigned char *d=(volatile unsigned char*)p;
    while(n--)*d++=(unsigned char)c;
    return p;
}
void *memcpy(void *p,const void *q,size_t n){
    volatile unsigned char *d=(volatile unsigned char*)p;
    const unsigned char *s=(const unsigned char*)q;
    while(n--)*d++=*s++;
    return p;
}
#endif
static int same_string(const char *a,const char *b){
    while(*a&&*a==*b){a++;b++;}
    return *a==*b;
}
typedef struct {uint32_t x,y,w,h;} View;
static View viewport(uint32_t w,uint32_t h){
    View v={0,0,w,w*3/4};
    if(v.h>h){v.h=h;v.w=h*4/3;}
    v.x=(w-v.w)/2;v.y=(h-v.h)/2;return v;
}
static uint32_t rgba565(uint16_t p){
    uint32_t r=(p>>11)&31,g=(p>>5)&63,b=p&31;
    return 0xff000000u|((b<<3|b>>2)<<16)|((g<<2|g>>4)<<8)|(r<<3|r>>2);
}

static uint32_t memtype(VkPhysicalDeviceMemoryProperties *m,uint32_t bits,VkMemoryPropertyFlags flags){
    for(uint32_t i=0;i<m->memoryTypeCount;i++)
        if((bits&(1u<<i))&&(m->memoryTypes[i].propertyFlags&flags)==flags)return i;
    return UINT32_MAX;
}

#define I(name) PFN_##name name=(PFN_##name)gipa(instance,#name)
#define CHECK(expr) do{VkResult v_=(expr);if(v_!=VK_SUCCESS){report(#expr,(int)v_);goto done;}}while(0)
#define REQUIRE(p) do{if(!(p)){report("Missing Vulkan function: " #p,1);goto done;}}while(0)
#define W 640u
#define H 400u
static HMODULE dll;
static PFN_vkGetInstanceProcAddr gipa;
static VkInstance instance;
static VkSurfaceKHR surface;
static VkDevice device;
static VkSwapchainKHR swap;
static VkBuffer buffer;
static VkImage source,images[32];
static VkDeviceMemory bm,im;
static VkCommandPool pool;
static VkSemaphore acquired,finished[32];
static VkFence fence;
static VkCommandBuffer cmd;
static VkQueue queue;
static uint32_t count,finish_count;
static unsigned submitted;
static int ready,linear_filter;
static VkFormat upload_format;
static unsigned upload_bytes;
static HWND hwnd;
static void *mapped;
static VkExtent2D extent;
static View v;
static PFN_vkUnmapMemory unmap_memory;
static PFN_vkDestroyInstance destroy_instance;
static PFN_vkDestroySurfaceKHR destroy_surface;
static PFN_vkGetDeviceProcAddr gdpa;
static PFN_vkDestroyDevice destroy_device;
static PFN_vkDeviceWaitIdle wait_idle;
static MatroxVkReport reporter;
#ifdef MATROX_VK_SHADOW
static VkPhysicalDevice shadow_physical;
static unsigned shadow_family,shadow_attempted,shadow_ready;
#endif
static void report(const char *msg,int code){if(reporter)reporter(msg,code);}
static PFN_vkGetDeviceQueue vkGetDeviceQueue;
static PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
static PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
static PFN_vkCreateBuffer vkCreateBuffer;
static PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
static PFN_vkAllocateMemory vkAllocateMemory;
static PFN_vkBindBufferMemory vkBindBufferMemory;
static PFN_vkMapMemory vkMapMemory;
static PFN_vkUnmapMemory vkUnmapMemory;
static PFN_vkCreateImage vkCreateImage;
static PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
static PFN_vkBindImageMemory vkBindImageMemory;
static PFN_vkCreateCommandPool vkCreateCommandPool;
static PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
static PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
static PFN_vkEndCommandBuffer vkEndCommandBuffer;
static PFN_vkResetCommandBuffer vkResetCommandBuffer;
static PFN_vkResetFences vkResetFences;
static PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
static PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
static PFN_vkCmdClearColorImage vkCmdClearColorImage;
static PFN_vkCmdBlitImage vkCmdBlitImage;
static PFN_vkCreateSemaphore vkCreateSemaphore;
static PFN_vkCreateFence vkCreateFence;
static PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
static PFN_vkQueueSubmit vkQueueSubmit;
static PFN_vkWaitForFences vkWaitForFences;
static PFN_vkQueuePresentKHR vkQueuePresentKHR;

void matrox_vk_close(void){
    if(device&&gdpa){
        if(wait_idle){
            VkResult wr=wait_idle(device);
            if(wr!=VK_SUCCESS&&wr!=VK_ERROR_DEVICE_LOST){
                ready=0;report("Vulkan shutdown deferred; device idle uncertain",(int)wr);return;
            }
        }
#ifdef MATROX_VK_SHADOW
        VkResult sr=msi_vk_bridge_close();
        shadow_ready=0;
        if(sr!=VK_SUCCESS&&sr!=VK_ERROR_DEVICE_LOST){
            ready=0;report("Vulkan shadow shutdown deferred",(int)sr);return;
        }
        shadow_physical=VK_NULL_HANDLE;shadow_family=shadow_attempted=0;
#endif
        #define DESTROY(fn,obj) do{if(obj){PFN_##fn f=(PFN_##fn)gdpa(device,#fn);if(f)f(device,obj,NULL);}}while(0)
        if(mapped&&unmap_memory)unmap_memory(device,bm);
        DESTROY(vkDestroyFence,fence);for(uint32_t i=0;i<finish_count;i++){DESTROY(vkDestroySemaphore,finished[i]);}DESTROY(vkDestroySemaphore,acquired);
        DESTROY(vkDestroyCommandPool,pool);DESTROY(vkDestroyImage,source);DESTROY(vkDestroyBuffer,buffer);
        DESTROY(vkFreeMemory,im);DESTROY(vkFreeMemory,bm);DESTROY(vkDestroySwapchainKHR,swap);
        if(destroy_device)destroy_device(device,NULL);
    }
    if(surface&&destroy_surface)destroy_surface(instance,surface,NULL);

    if(instance&&destroy_instance)destroy_instance(instance,NULL);
    if(dll)FreeLibrary(dll);
    dll=NULL;gipa=NULL;instance=VK_NULL_HANDLE;surface=VK_NULL_HANDLE;device=VK_NULL_HANDLE;
    swap=VK_NULL_HANDLE;buffer=VK_NULL_HANDLE;source=VK_NULL_HANDLE;bm=im=VK_NULL_HANDLE;
    pool=VK_NULL_HANDLE;acquired=VK_NULL_HANDLE;fence=VK_NULL_HANDLE;cmd=VK_NULL_HANDLE;queue=VK_NULL_HANDLE;
    count=finish_count=submitted=0;mapped=NULL;ready=0;hwnd=NULL;
    unmap_memory=NULL;destroy_instance=NULL;destroy_surface=NULL;gdpa=NULL;destroy_device=NULL;wait_idle=NULL;
    reporter=NULL;
}
int matrox_vk_open(void *window,int linear,MatroxVkReport logger){
    matrox_vk_close();
    if(device){if(logger)logger("Vulkan prior device still pending",1);return 1;}
    reporter=logger;hwnd=(HWND)window;linear_filter=linear!=0;
    int rc=1;
    if(!hwnd||!IsWindow(hwnd))goto done;
    dll=LoadLibraryExW(L"vulkan-1.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!dll){rc=77;report("Vulkan loader unavailable",77);goto done;}
    gipa=(PFN_vkGetInstanceProcAddr)GetProcAddress(dll,"vkGetInstanceProcAddr");
    if(!gipa)goto done;
    PFN_vkCreateInstance create_instance=(PFN_vkCreateInstance)gipa(NULL,"vkCreateInstance");
    REQUIRE(create_instance);
    const char *extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkApplicationInfo app={VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="Scorched Planet Matrox presentation probe";app.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo=&app;ci.enabledExtensionCount=2;ci.ppEnabledExtensionNames=extensions;
    VkResult ir=create_instance(&ci,NULL,&instance);
    if(ir==VK_ERROR_INCOMPATIBLE_DRIVER||ir==VK_ERROR_EXTENSION_NOT_PRESENT){rc=77;report("Vulkan instance unavailable",(int)ir);goto done;}
    if(ir!=VK_SUCCESS){report("vkCreateInstance",(int)ir);goto done;}
    destroy_instance=(PFN_vkDestroyInstance)gipa(instance,"vkDestroyInstance");
    REQUIRE(destroy_instance);
    I(vkCreateWin32SurfaceKHR);I(vkEnumeratePhysicalDevices);
    I(vkGetPhysicalDeviceProperties);I(vkGetPhysicalDeviceQueueFamilyProperties);
    I(vkGetPhysicalDeviceSurfaceSupportKHR);I(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    I(vkGetPhysicalDeviceSurfaceFormatsKHR);I(vkGetPhysicalDeviceFormatProperties);I(vkGetPhysicalDeviceImageFormatProperties);
    I(vkEnumerateDeviceExtensionProperties);I(vkGetPhysicalDeviceMemoryProperties);I(vkCreateDevice);
    destroy_surface=(PFN_vkDestroySurfaceKHR)gipa(instance,"vkDestroySurfaceKHR");
    gdpa=(PFN_vkGetDeviceProcAddr)gipa(instance,"vkGetDeviceProcAddr");
    REQUIRE(vkCreateWin32SurfaceKHR);REQUIRE(vkEnumeratePhysicalDevices);
    REQUIRE(vkGetPhysicalDeviceProperties);REQUIRE(vkGetPhysicalDeviceQueueFamilyProperties);
    REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR);REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR);REQUIRE(vkGetPhysicalDeviceFormatProperties);REQUIRE(vkGetPhysicalDeviceImageFormatProperties);
    REQUIRE(vkEnumerateDeviceExtensionProperties);REQUIRE(vkGetPhysicalDeviceMemoryProperties);
    REQUIRE(vkCreateDevice);REQUIRE(destroy_surface);REQUIRE(gdpa);
    VkWin32SurfaceCreateInfoKHR si={VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    si.hinstance=GetModuleHandleW(NULL);si.hwnd=hwnd;
    CHECK(vkCreateWin32SurfaceKHR(instance,&si,NULL,&surface));
    static VkPhysicalDevice devices[32];VkPhysicalDevice physical=VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR chosen={VK_FORMAT_UNDEFINED,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    uint32_t n=32,family=0;
    CHECK(vkEnumeratePhysicalDevices(instance,&n,devices));
    for(uint32_t i=0;i<n&&!physical;i++){
        static VkPhysicalDeviceProperties prop;vkGetPhysicalDeviceProperties(devices[i],&prop);
        report(prop.deviceName,(int)prop.deviceType);
        if(prop.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
        VkFormatProperties fmt;
        VkFormatFeatureFlags need=VK_FORMAT_FEATURE_BLIT_SRC_BIT;
        if(linear_filter)need|=VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        const VkFormat candidates[]={VK_FORMAT_R5G6B5_UNORM_PACK16,VK_FORMAT_R8G8B8A8_UNORM};
        VkFormat candidate=VK_FORMAT_UNDEFINED;
        for(unsigned k=0;k<2;k++){
            VkImageFormatProperties ip;
            vkGetPhysicalDeviceFormatProperties(devices[i],candidates[k],&fmt);
            if((fmt.optimalTilingFeatures&need)!=need)continue;
            if(vkGetPhysicalDeviceImageFormatProperties(devices[i],candidates[k],VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,0,&ip)!=VK_SUCCESS)continue;
            if(ip.maxExtent.width<W||ip.maxExtent.height<H||!(ip.sampleCounts&VK_SAMPLE_COUNT_1_BIT))continue;
            candidate=candidates[k];break;
        }
        if(candidate==VK_FORMAT_UNDEFINED)continue;
        static VkExtensionProperties ex[256];uint32_t en=256;int has_swap=0;
        if(vkEnumerateDeviceExtensionProperties(devices[i],NULL,&en,ex)!=VK_SUCCESS)continue;
        for(uint32_t j=0;j<en;j++)if(same_string(ex[j].extensionName,VK_KHR_SWAPCHAIN_EXTENSION_NAME))has_swap=1;
        if(!has_swap)continue;
        if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[i],surface,&caps)!=VK_SUCCESS||
           !(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT))continue;
        static VkSurfaceFormatKHR formats[64];uint32_t fn=64;
        if(vkGetPhysicalDeviceSurfaceFormatsKHR(devices[i],surface,&fn,formats)!=VK_SUCCESS)continue;
        chosen.format=VK_FORMAT_UNDEFINED;
        for(uint32_t j=0;j<fn;j++){
            VkSurfaceFormatKHR f=formats[j];
            if(f.format==VK_FORMAT_UNDEFINED)f.format=VK_FORMAT_B8G8R8A8_UNORM;
            if(f.format!=VK_FORMAT_B8G8R8A8_UNORM&&f.format!=VK_FORMAT_R8G8B8A8_UNORM)continue;
            vkGetPhysicalDeviceFormatProperties(devices[i],f.format,&fmt);
            if(fmt.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT){chosen=f;break;}
        }
        if(chosen.format==VK_FORMAT_UNDEFINED)continue;
        static VkQueueFamilyProperties queues[64];uint32_t qn=64;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i],&qn,queues);
        for(uint32_t j=0;j<qn;j++){
            VkBool32 support=0;
            if(queues[j].queueCount&&(queues[j].queueFlags&VK_QUEUE_GRAPHICS_BIT)&&
               vkGetPhysicalDeviceSurfaceSupportKHR(devices[i],j,surface,&support)==VK_SUCCESS&&support){
                physical=devices[i];family=j;upload_format=candidate;report("VULKAN_SELECTED",0);report(prop.deviceName,(int)j);break;
            }
        }
    }
    if(!physical){rc=77;report("No eligible GPU/surface",77);goto done;}
    /* From this point an execution error is a failed test, not a skip. */
    rc=1;
    float priority=1.0f;
    VkDeviceQueueCreateInfo qi={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
    const char *de[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo di={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;di.enabledExtensionCount=1;di.ppEnabledExtensionNames=de;
    CHECK(vkCreateDevice(physical,&di,NULL,&device));
    destroy_device=(PFN_vkDestroyDevice)gdpa(device,"vkDestroyDevice");
    wait_idle=(PFN_vkDeviceWaitIdle)gdpa(device,"vkDeviceWaitIdle");
    REQUIRE(destroy_device);REQUIRE(wait_idle);
    vkGetDeviceQueue=(PFN_vkGetDeviceQueue)gdpa(device,"vkGetDeviceQueue");vkCreateSwapchainKHR=(PFN_vkCreateSwapchainKHR)gdpa(device,"vkCreateSwapchainKHR");vkGetSwapchainImagesKHR=(PFN_vkGetSwapchainImagesKHR)gdpa(device,"vkGetSwapchainImagesKHR");
    vkCreateBuffer=(PFN_vkCreateBuffer)gdpa(device,"vkCreateBuffer");vkGetBufferMemoryRequirements=(PFN_vkGetBufferMemoryRequirements)gdpa(device,"vkGetBufferMemoryRequirements");vkAllocateMemory=(PFN_vkAllocateMemory)gdpa(device,"vkAllocateMemory");vkBindBufferMemory=(PFN_vkBindBufferMemory)gdpa(device,"vkBindBufferMemory");vkMapMemory=(PFN_vkMapMemory)gdpa(device,"vkMapMemory");vkUnmapMemory=(PFN_vkUnmapMemory)gdpa(device,"vkUnmapMemory");
    vkCreateImage=(PFN_vkCreateImage)gdpa(device,"vkCreateImage");vkGetImageMemoryRequirements=(PFN_vkGetImageMemoryRequirements)gdpa(device,"vkGetImageMemoryRequirements");vkBindImageMemory=(PFN_vkBindImageMemory)gdpa(device,"vkBindImageMemory");
    vkCreateCommandPool=(PFN_vkCreateCommandPool)gdpa(device,"vkCreateCommandPool");vkAllocateCommandBuffers=(PFN_vkAllocateCommandBuffers)gdpa(device,"vkAllocateCommandBuffers");vkBeginCommandBuffer=(PFN_vkBeginCommandBuffer)gdpa(device,"vkBeginCommandBuffer");vkEndCommandBuffer=(PFN_vkEndCommandBuffer)gdpa(device,"vkEndCommandBuffer");vkResetCommandBuffer=(PFN_vkResetCommandBuffer)gdpa(device,"vkResetCommandBuffer");vkResetFences=(PFN_vkResetFences)gdpa(device,"vkResetFences");
    vkCmdPipelineBarrier=(PFN_vkCmdPipelineBarrier)gdpa(device,"vkCmdPipelineBarrier");vkCmdCopyBufferToImage=(PFN_vkCmdCopyBufferToImage)gdpa(device,"vkCmdCopyBufferToImage");vkCmdClearColorImage=(PFN_vkCmdClearColorImage)gdpa(device,"vkCmdClearColorImage");vkCmdBlitImage=(PFN_vkCmdBlitImage)gdpa(device,"vkCmdBlitImage");
    vkCreateSemaphore=(PFN_vkCreateSemaphore)gdpa(device,"vkCreateSemaphore");vkCreateFence=(PFN_vkCreateFence)gdpa(device,"vkCreateFence");vkAcquireNextImageKHR=(PFN_vkAcquireNextImageKHR)gdpa(device,"vkAcquireNextImageKHR");vkQueueSubmit=(PFN_vkQueueSubmit)gdpa(device,"vkQueueSubmit");vkWaitForFences=(PFN_vkWaitForFences)gdpa(device,"vkWaitForFences");vkQueuePresentKHR=(PFN_vkQueuePresentKHR)gdpa(device,"vkQueuePresentKHR");
    REQUIRE(vkGetDeviceQueue);REQUIRE(vkCreateSwapchainKHR);REQUIRE(vkGetSwapchainImagesKHR);
    REQUIRE(vkCreateBuffer);REQUIRE(vkGetBufferMemoryRequirements);REQUIRE(vkAllocateMemory);REQUIRE(vkBindBufferMemory);REQUIRE(vkMapMemory);REQUIRE(vkUnmapMemory);
    REQUIRE(vkCreateImage);REQUIRE(vkGetImageMemoryRequirements);REQUIRE(vkBindImageMemory);
    REQUIRE(vkCreateCommandPool);REQUIRE(vkAllocateCommandBuffers);REQUIRE(vkBeginCommandBuffer);REQUIRE(vkEndCommandBuffer);REQUIRE(vkResetCommandBuffer);REQUIRE(vkResetFences);
    REQUIRE(vkCmdPipelineBarrier);REQUIRE(vkCmdCopyBufferToImage);REQUIRE(vkCmdClearColorImage);REQUIRE(vkCmdBlitImage);
    REQUIRE(vkCreateSemaphore);REQUIRE(vkCreateFence);REQUIRE(vkAcquireNextImageKHR);REQUIRE(vkQueueSubmit);REQUIRE(vkWaitForFences);REQUIRE(vkQueuePresentKHR);
    vkGetDeviceQueue(device,family,0,&queue);
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps));
    extent=caps.currentExtent;
    if(extent.width==UINT32_MAX){
        RECT r;if(!GetClientRect(hwnd,&r))goto done;
        extent.width=(uint32_t)(r.right-r.left);extent.height=(uint32_t)(r.bottom-r.top);
        if(extent.width<caps.minImageExtent.width)extent.width=caps.minImageExtent.width;
        if(extent.height<caps.minImageExtent.height)extent.height=caps.minImageExtent.height;
        if(extent.width>caps.maxImageExtent.width)extent.width=caps.maxImageExtent.width;
        if(extent.height>caps.maxImageExtent.height)extent.height=caps.maxImageExtent.height;
    }
    if(!extent.width||!extent.height||extent.width>8192||extent.height>8192)goto done;
    v=viewport(extent.width,extent.height);if(!v.w||!v.h)goto done;
    VkSwapchainCreateInfoKHR sc={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sc.surface=surface;sc.minImageCount=caps.minImageCount;sc.imageFormat=chosen.format;sc.imageColorSpace=chosen.colorSpace;
    sc.imageExtent=extent;sc.imageArrayLayers=1;sc.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;sc.preTransform=caps.currentTransform;
    sc.compositeAlpha=(VkCompositeAlphaFlagBitsKHR)(caps.supportedCompositeAlpha&(~caps.supportedCompositeAlpha+1u));
    sc.presentMode=VK_PRESENT_MODE_FIFO_KHR;sc.clipped=VK_TRUE;
    CHECK(vkCreateSwapchainKHR(device,&sc,NULL,&swap));
    count=32;CHECK(vkGetSwapchainImagesKHR(device,swap,&count,images));
    static VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(physical,&mp);
    VkBufferCreateInfo bc={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    upload_bytes=upload_format==VK_FORMAT_R5G6B5_UNORM_PACK16?2u:4u;
    bc.size=W*H*upload_bytes;bc.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;bc.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    CHECK(vkCreateBuffer(device,&bc,NULL,&buffer));
    VkMemoryRequirements mr;vkGetBufferMemoryRequirements(device,buffer,&mr);
    VkMemoryAllocateInfo ma={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ma.allocationSize=mr.size;ma.memoryTypeIndex=memtype(&mp,mr.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(ma.memoryTypeIndex==UINT32_MAX)goto done;
    CHECK(vkAllocateMemory(device,&ma,NULL,&bm));CHECK(vkBindBufferMemory(device,buffer,bm,0));
    unmap_memory=vkUnmapMemory;
    CHECK(vkMapMemory(device,bm,0,bc.size,0,&mapped));
    
    VkImageCreateInfo ic={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ic.imageType=VK_IMAGE_TYPE_2D;ic.format=upload_format;ic.extent.width=W;ic.extent.height=H;ic.extent.depth=1;
    ic.mipLevels=1;ic.arrayLayers=1;ic.samples=VK_SAMPLE_COUNT_1_BIT;ic.tiling=VK_IMAGE_TILING_OPTIMAL;
    ic.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;ic.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    ic.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    CHECK(vkCreateImage(device,&ic,NULL,&source));vkGetImageMemoryRequirements(device,source,&mr);
    ma.allocationSize=mr.size;ma.memoryTypeIndex=memtype(&mp,mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(ma.memoryTypeIndex==UINT32_MAX)goto done;
    CHECK(vkAllocateMemory(device,&ma,NULL,&im));CHECK(vkBindImageMemory(device,source,im,0));
    VkCommandPoolCreateInfo pc={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=family;pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CHECK(vkCreateCommandPool(device,&pc,NULL,&pool));
    VkCommandBufferAllocateInfo ca={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
    CHECK(vkAllocateCommandBuffers(device,&ca,&cmd));
    VkSemaphoreCreateInfo se={VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fe={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    CHECK(vkCreateSemaphore(device,&se,NULL,&acquired));CHECK(vkCreateFence(device,&fe,NULL,&fence));
    for(uint32_t i=0;i<count;i++){CHECK(vkCreateSemaphore(device,&se,NULL,&finished[i]));finish_count++;}

    report(upload_bytes==2?"VULKAN_UPLOAD RGB565 native":"VULKAN_UPLOAD RGBA8 converted",(int)bc.size);
    report(linear_filter?"VULKAN_FILTER linear":"VULKAN_FILTER nearest",0);
#ifdef MATROX_VK_SHADOW
    shadow_physical=physical;shadow_family=family;
#endif
    ready=1;report("VULKAN_PRESENTER_READY",0);return 0;
done:
    matrox_vk_close();return rc;
}
#ifdef MATROX_VK_SHADOW
int matrox_vk_shadow_compare(const MsiGpuDrawBatch *batch,const uint16_t *before,
    const uint16_t *cpu,uint16_t *scratch,MsiGpuCompare *stats){
    if(!ready||!shadow_physical)return VK_NOT_READY;
    if(!shadow_attempted){
        shadow_attempted=1;
        VkResult r=msi_vk_bridge_open(instance,shadow_physical,device,queue,shadow_family,gipa,
            world_vert,sizeof(world_vert),world_frag,sizeof(world_frag));
        if(r!=VK_SUCCESS){
            report("VULKAN_SHADOW unavailable",(int)r);
            if(r==VK_ERROR_DEVICE_LOST)ready=0;
            return r;
        }
        shadow_ready=1;report("VULKAN_SHADOW ready",0);
    }
    if(!shadow_ready)return VK_ERROR_FEATURE_NOT_PRESENT;
    VkResult r=msi_vk_bridge_compare(batch,before,cpu,scratch,stats);
    if(r!=VK_SUCCESS){
        shadow_ready=0;report("VULKAN_SHADOW disabled",(int)r);
        VkResult close_result=msi_vk_bridge_close();
        if(r==VK_ERROR_DEVICE_LOST||close_result!=VK_SUCCESS)ready=0;
    }
    return r;
}
static int matrox_vk_shadow_tagged_impl(const MsiGpuDrawBatch *batch,uint32_t *scratch,unsigned *region_pixels){
    if(!ready||!shadow_ready)return VK_NOT_READY;
    VkResult r=region_pixels?msi_vk_bridge_tagged_roi(batch,scratch,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag),region_pixels):
        msi_vk_bridge_tagged(batch,scratch,world_vert,sizeof(world_vert),world_frag,sizeof(world_frag));
    if(r!=VK_SUCCESS){
        shadow_ready=0;report("VULKAN_TAGGED disabled",(int)r);
        VkResult close_result=msi_vk_bridge_close();
        if(r==VK_ERROR_DEVICE_LOST||close_result!=VK_SUCCESS)ready=0;
    }
    return r;
}
int matrox_vk_shadow_tagged(const MsiGpuDrawBatch *batch,uint32_t *scratch){
    return matrox_vk_shadow_tagged_impl(batch,scratch,NULL);
}
int matrox_vk_shadow_tagged_roi(const MsiGpuDrawBatch *batch,uint32_t *scratch,unsigned *region_pixels){
    if(!region_pixels)return VK_ERROR_INITIALIZATION_FAILED;
    return matrox_vk_shadow_tagged_impl(batch,scratch,region_pixels);
}
#endif
int matrox_vk_present(const uint16_t *native_pixels,uint32_t pitch){
    if(!ready||!native_pixels||pitch<W*2u||pitch%2u)return 1;
    RECT client;
    if(!IsWindow(hwnd)||!GetClientRect(hwnd,&client))return 1;
    if(IsIconic(hwnd)||client.right<=client.left||client.bottom<=client.top)return 2;
    if((uint32_t)(client.right-client.left)!=extent.width||(uint32_t)(client.bottom-client.top)!=extent.height){
        report("Vulkan surface extent changed; recreate",3);return 3;
    }
    /* Wait only at reuse, not immediately after submission. CPU rasterization
     * of the next native frame can overlap GPU work on the previous staging
     * image. The guest buffer itself is never mapped into Vulkan. */
    if(submitted){CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ull));}
    if(upload_bytes==2){
        uint16_t *pixels=(uint16_t*)mapped;
        for(uint32_t y=0;y<H;y++){
            const uint16_t *row=(const uint16_t*)((const unsigned char*)native_pixels+y*pitch);
            for(uint32_t x=0;x<W;x++)pixels[y*W+x]=row[x];
        }
    }else{
        uint32_t *pixels=(uint32_t*)mapped;
        for(uint32_t y=0;y<H;y++){
            const uint16_t *row=(const uint16_t*)((const unsigned char*)native_pixels+y*pitch);
            for(uint32_t x=0;x<W;x++)pixels[y*W+x]=rgba565(row[x]);
        }
    }
    uint32_t index=0;VkResult ar=vkAcquireNextImageKHR(device,swap,1000000000ull,acquired,VK_NULL_HANDLE,&index);
    if(ar==VK_ERROR_OUT_OF_DATE_KHR){report("vkAcquireNextImageKHR out of date",3);return 3;}
    if((ar!=VK_SUCCESS&&ar!=VK_SUBOPTIMAL_KHR)||index>=count){report("vkAcquireNextImageKHR",(int)ar);goto done;}
    if(submitted){CHECK(vkResetFences(device,1,&fence));CHECK(vkResetCommandBuffer(cmd,0));}
    VkCommandBufferBeginInfo cb={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHECK(vkBeginCommandBuffer(cmd,&cb));
    VkImageSubresourceRange range={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    VkImageMemoryBarrier barrier={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.subresourceRange=range;
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.image=source;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    VkBufferImageCopy copy={0};copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;copy.imageSubresource.layerCount=1;
    copy.imageExtent.width=W;copy.imageExtent.height=H;copy.imageExtent.depth=1;vkCmdCopyBufferToImage(cmd,buffer,source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    barrier.image=images[index];barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask=0;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    VkClearColorValue black={{0.f,0.f,0.f,1.f}};
    vkCmdClearColorImage(cmd,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&black,1,&range);
    barrier.oldLayout=barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask=barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    VkImageBlit blit={0};blit.srcSubresource.aspectMask=blit.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount=blit.dstSubresource.layerCount=1;
    blit.srcOffsets[1].x=W;blit.srcOffsets[1].y=H;blit.srcOffsets[1].z=1;
    blit.dstOffsets[0].x=(int32_t)v.x;blit.dstOffsets[0].y=(int32_t)v.y;
    blit.dstOffsets[1].x=(int32_t)(v.x+v.w);blit.dstOffsets[1].y=(int32_t)(v.y+v.h);blit.dstOffsets[1].z=1;
    vkCmdBlitImage(cmd,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,linear_filter?VK_FILTER_LINEAR:VK_FILTER_NEAREST);
    barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;barrier.dstAccessMask=0;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);
    CHECK(vkEndCommandBuffer(cmd));
    VkPipelineStageFlags wait_stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit={VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired;
    submit.pWaitDstStageMask=&wait_stage;submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
    submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&finished[index];
    CHECK(vkQueueSubmit(queue,1,&submit,fence));
    /* Present waits on this image's signal semaphore, not the CPU. */
    VkPresentInfoKHR present={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount=1;present.pWaitSemaphores=&finished[index];present.swapchainCount=1;present.pSwapchains=&swap;present.pImageIndices=&index;
    VkResult pr=vkQueuePresentKHR(queue,&present);
    if(pr==VK_ERROR_OUT_OF_DATE_KHR){report("vkQueuePresentKHR out of date",3);return 3;}
    if(pr!=VK_SUCCESS&&pr!=VK_SUBOPTIMAL_KHR){report("vkQueuePresentKHR",(int)pr);goto done;}

    submitted=1;return 0;
done:
    ready=0;return 1;
}
