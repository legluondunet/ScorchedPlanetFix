/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent x86 Win32 Vulkan presentation proof. No game code/assets.
 * One synthetic 640x400 RGB565 image -> GPU RGBA8 texture -> linear
 * swapchain blit, with centered 4:3 viewport. Not the MSI95 backend yet.
 */
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
static int selftest(void){
    View v=viewport(1920,1080),p=viewport(800,1000);
    if(v.x!=240||v.y!=0||v.w!=1440||v.h!=1080||
       p.x!=0||p.y!=200||p.w!=800||p.h!=600||
       rgba565(0)!=0xff000000u||rgba565(0xffff)!=0xffffffffu||
       rgba565(0xf800)!=0xff0000ffu||rgba565(0x07e0)!=0xff00ff00u||
       rgba565(0x001f)!=0xffff0000u)return 1;
    puts("VULKAN_PROBE_CPU_SELFTEST_PASS");return 0;
}
static uint32_t memtype(VkPhysicalDeviceMemoryProperties *m,uint32_t bits,VkMemoryPropertyFlags flags){
    for(uint32_t i=0;i<m->memoryTypeCount;i++)
        if((bits&(1u<<i))&&(m->memoryTypes[i].propertyFlags&flags)==flags)return i;
    return UINT32_MAX;
}
#define I(name) PFN_##name name=(PFN_##name)gipa(instance,#name)
#define D(name) PFN_##name name=(PFN_##name)gdpa(device,#name)
#define CHECK(expr) do{VkResult v_=(expr);if(v_!=VK_SUCCESS){fprintf(stderr,"%s: %d\n",#expr,(int)v_);goto done;}}while(0)
#define REQUIRE(p) do{if(!(p)){fprintf(stderr,"Missing Vulkan function: %s\n",#p);goto done;}}while(0)
#define W 640u
#define H 400u

static int probe(void){
    const unsigned frames=180;
    int rc=77; /* unavailable before logical device, not a successful GPU test */
    HMODULE dll=LoadLibraryExW(L"vulkan-1.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!dll){puts("VULKAN_PROBE_UNAVAILABLE loader");return rc;}
    PFN_vkGetInstanceProcAddr gipa=(PFN_vkGetInstanceProcAddr)GetProcAddress(dll,"vkGetInstanceProcAddr");
    VkInstance instance=VK_NULL_HANDLE;
    VkSurfaceKHR surface=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkSwapchainKHR swap=VK_NULL_HANDLE;
    VkBuffer buffer=VK_NULL_HANDLE;
    VkImage source=VK_NULL_HANDLE;
    VkDeviceMemory bm=VK_NULL_HANDLE,im=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkSemaphore acquired=VK_NULL_HANDLE,finished[32]={0};
    uint32_t finish_count=0;
    void *mapped=NULL;
    PFN_vkUnmapMemory unmap_memory=NULL;
    VkFence fence=VK_NULL_HANDLE;
    HWND hwnd=NULL;
    PFN_vkDestroyInstance destroy_instance=NULL;
    PFN_vkDestroySurfaceKHR destroy_surface=NULL;
    PFN_vkGetDeviceProcAddr gdpa=NULL;
    PFN_vkDestroyDevice destroy_device=NULL;
    PFN_vkDeviceWaitIdle wait_idle=NULL;
    rc=1;
    if(!gipa)goto done;
    PFN_vkCreateInstance create_instance=(PFN_vkCreateInstance)gipa(NULL,"vkCreateInstance");
    REQUIRE(create_instance);
    const char *extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkApplicationInfo app={VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="Scorched Planet Matrox presentation probe";app.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo=&app;ci.enabledExtensionCount=2;ci.ppEnabledExtensionNames=extensions;
    VkResult ir=create_instance(&ci,NULL,&instance);
    if(ir==VK_ERROR_INCOMPATIBLE_DRIVER||ir==VK_ERROR_EXTENSION_NOT_PRESENT){rc=77;fprintf(stderr,"Vulkan instance unavailable: %d\n",(int)ir);goto done;}
    if(ir!=VK_SUCCESS){fprintf(stderr,"vkCreateInstance: %d\n",(int)ir);goto done;}
    destroy_instance=(PFN_vkDestroyInstance)gipa(instance,"vkDestroyInstance");
    REQUIRE(destroy_instance);
    I(vkCreateWin32SurfaceKHR);I(vkEnumeratePhysicalDevices);
    I(vkGetPhysicalDeviceProperties);I(vkGetPhysicalDeviceQueueFamilyProperties);
    I(vkGetPhysicalDeviceSurfaceSupportKHR);I(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    I(vkGetPhysicalDeviceSurfaceFormatsKHR);I(vkGetPhysicalDeviceFormatProperties);
    I(vkEnumerateDeviceExtensionProperties);I(vkGetPhysicalDeviceMemoryProperties);I(vkCreateDevice);
    destroy_surface=(PFN_vkDestroySurfaceKHR)gipa(instance,"vkDestroySurfaceKHR");
    gdpa=(PFN_vkGetDeviceProcAddr)gipa(instance,"vkGetDeviceProcAddr");
    REQUIRE(vkCreateWin32SurfaceKHR);REQUIRE(vkEnumeratePhysicalDevices);
    REQUIRE(vkGetPhysicalDeviceProperties);REQUIRE(vkGetPhysicalDeviceQueueFamilyProperties);
    REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR);REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR);REQUIRE(vkGetPhysicalDeviceFormatProperties);
    REQUIRE(vkEnumerateDeviceExtensionProperties);REQUIRE(vkGetPhysicalDeviceMemoryProperties);
    REQUIRE(vkCreateDevice);REQUIRE(destroy_surface);REQUIRE(gdpa);
    hwnd=CreateWindowExW(0,L"STATIC",L"Matrox Vulkan synthetic presentation",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                         CW_USEDEFAULT,CW_USEDEFAULT,960,720,NULL,NULL,GetModuleHandleW(NULL),NULL);
    if(!hwnd){rc=1;goto done;}
    VkWin32SurfaceCreateInfoKHR si={VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    si.hinstance=GetModuleHandleW(NULL);si.hwnd=hwnd;
    CHECK(vkCreateWin32SurfaceKHR(instance,&si,NULL,&surface));
    VkPhysicalDevice devices[32],physical=VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR chosen={VK_FORMAT_UNDEFINED,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    uint32_t n=32,family=0;
    CHECK(vkEnumeratePhysicalDevices(instance,&n,devices));
    for(uint32_t i=0;i<n&&!physical;i++){
        VkPhysicalDeviceProperties prop;vkGetPhysicalDeviceProperties(devices[i],&prop);
        printf("VULKAN_DEVICE %s type=%d\n",prop.deviceName,(int)prop.deviceType);
        if(prop.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
        VkFormatProperties fmt;vkGetPhysicalDeviceFormatProperties(devices[i],VK_FORMAT_R8G8B8A8_UNORM,&fmt);
        VkFormatFeatureFlags need=VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if((fmt.optimalTilingFeatures&need)!=need)continue;
        VkExtensionProperties ex[256];uint32_t en=256;int has_swap=0;
        if(vkEnumerateDeviceExtensionProperties(devices[i],NULL,&en,ex)!=VK_SUCCESS)continue;
        for(uint32_t j=0;j<en;j++)if(!strcmp(ex[j].extensionName,VK_KHR_SWAPCHAIN_EXTENSION_NAME))has_swap=1;
        if(!has_swap)continue;
        if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[i],surface,&caps)!=VK_SUCCESS||
           !(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT))continue;
        VkSurfaceFormatKHR formats[64];uint32_t fn=64;
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
        VkQueueFamilyProperties queues[64];uint32_t qn=64;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i],&qn,queues);
        for(uint32_t j=0;j<qn;j++){
            VkBool32 support=0;
            if(queues[j].queueCount&&(queues[j].queueFlags&VK_QUEUE_GRAPHICS_BIT)&&
               vkGetPhysicalDeviceSurfaceSupportKHR(devices[i],j,surface,&support)==VK_SUCCESS&&support){
                physical=devices[i];family=j;printf("VULKAN_SELECTED %s queue=%u\n",prop.deviceName,j);break;
            }
        }
    }
    if(!physical){rc=77;puts("VULKAN_PROBE_UNAVAILABLE no eligible GPU/surface");goto done;}
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
    D(vkGetDeviceQueue);D(vkCreateSwapchainKHR);D(vkGetSwapchainImagesKHR);
    D(vkCreateBuffer);D(vkGetBufferMemoryRequirements);D(vkAllocateMemory);D(vkBindBufferMemory);D(vkMapMemory);D(vkUnmapMemory);
    D(vkCreateImage);D(vkGetImageMemoryRequirements);D(vkBindImageMemory);
    D(vkCreateCommandPool);D(vkAllocateCommandBuffers);D(vkBeginCommandBuffer);D(vkEndCommandBuffer);D(vkResetCommandBuffer);D(vkResetFences);
    D(vkCmdPipelineBarrier);D(vkCmdCopyBufferToImage);D(vkCmdClearColorImage);D(vkCmdBlitImage);
    D(vkCreateSemaphore);D(vkCreateFence);D(vkAcquireNextImageKHR);D(vkQueueSubmit);D(vkWaitForFences);D(vkQueuePresentKHR);
    REQUIRE(vkGetDeviceQueue);REQUIRE(vkCreateSwapchainKHR);REQUIRE(vkGetSwapchainImagesKHR);
    REQUIRE(vkCreateBuffer);REQUIRE(vkGetBufferMemoryRequirements);REQUIRE(vkAllocateMemory);REQUIRE(vkBindBufferMemory);REQUIRE(vkMapMemory);REQUIRE(vkUnmapMemory);
    REQUIRE(vkCreateImage);REQUIRE(vkGetImageMemoryRequirements);REQUIRE(vkBindImageMemory);
    REQUIRE(vkCreateCommandPool);REQUIRE(vkAllocateCommandBuffers);REQUIRE(vkBeginCommandBuffer);REQUIRE(vkEndCommandBuffer);REQUIRE(vkResetCommandBuffer);REQUIRE(vkResetFences);
    REQUIRE(vkCmdPipelineBarrier);REQUIRE(vkCmdCopyBufferToImage);REQUIRE(vkCmdClearColorImage);REQUIRE(vkCmdBlitImage);
    REQUIRE(vkCreateSemaphore);REQUIRE(vkCreateFence);REQUIRE(vkAcquireNextImageKHR);REQUIRE(vkQueueSubmit);REQUIRE(vkWaitForFences);REQUIRE(vkQueuePresentKHR);
    VkQueue queue;vkGetDeviceQueue(device,family,0,&queue);
    ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps));
    VkExtent2D extent=caps.currentExtent;
    if(extent.width==UINT32_MAX){
        RECT r;if(!GetClientRect(hwnd,&r))goto done;
        extent.width=(uint32_t)(r.right-r.left);extent.height=(uint32_t)(r.bottom-r.top);
        if(extent.width<caps.minImageExtent.width)extent.width=caps.minImageExtent.width;
        if(extent.height<caps.minImageExtent.height)extent.height=caps.minImageExtent.height;
        if(extent.width>caps.maxImageExtent.width)extent.width=caps.maxImageExtent.width;
        if(extent.height>caps.maxImageExtent.height)extent.height=caps.maxImageExtent.height;
    }
    if(!extent.width||!extent.height||extent.width>8192||extent.height>8192)goto done;
    View v=viewport(extent.width,extent.height);if(!v.w||!v.h)goto done;
    VkSwapchainCreateInfoKHR sc={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sc.surface=surface;sc.minImageCount=caps.minImageCount;sc.imageFormat=chosen.format;sc.imageColorSpace=chosen.colorSpace;
    sc.imageExtent=extent;sc.imageArrayLayers=1;sc.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;sc.preTransform=caps.currentTransform;
    sc.compositeAlpha=(VkCompositeAlphaFlagBitsKHR)(caps.supportedCompositeAlpha&(~caps.supportedCompositeAlpha+1u));
    sc.presentMode=VK_PRESENT_MODE_FIFO_KHR;sc.clipped=VK_TRUE;
    CHECK(vkCreateSwapchainKHR(device,&sc,NULL,&swap));
    VkImage images[32];uint32_t count=32;CHECK(vkGetSwapchainImagesKHR(device,swap,&count,images));
    VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(physical,&mp);
    VkBufferCreateInfo bc={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bc.size=W*H*4;bc.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;bc.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    CHECK(vkCreateBuffer(device,&bc,NULL,&buffer));
    VkMemoryRequirements mr;vkGetBufferMemoryRequirements(device,buffer,&mr);
    VkMemoryAllocateInfo ma={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ma.allocationSize=mr.size;ma.memoryTypeIndex=memtype(&mp,mr.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(ma.memoryTypeIndex==UINT32_MAX)goto done;
    CHECK(vkAllocateMemory(device,&ma,NULL,&bm));CHECK(vkBindBufferMemory(device,buffer,bm,0));
    unmap_memory=vkUnmapMemory;
    CHECK(vkMapMemory(device,bm,0,bc.size,0,&mapped));
    uint32_t *pixels=(uint32_t*)mapped;
    VkImageCreateInfo ic={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ic.imageType=VK_IMAGE_TYPE_2D;ic.format=VK_FORMAT_R8G8B8A8_UNORM;ic.extent.width=W;ic.extent.height=H;ic.extent.depth=1;
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
    VkCommandBuffer cmd;CHECK(vkAllocateCommandBuffers(device,&ca,&cmd));
    VkSemaphoreCreateInfo se={VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fe={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    CHECK(vkCreateSemaphore(device,&se,NULL,&acquired));CHECK(vkCreateFence(device,&fe,NULL,&fence));
    for(uint32_t i=0;i<count;i++){CHECK(vkCreateSemaphore(device,&se,NULL,&finished[i]));finish_count++;}
    LARGE_INTEGER freq,t0,t1;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&t0);
    unsigned submitted=0;
    for(unsigned frame=0;frame<frames;frame++){
    MSG msg;
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
        if(msg.message==WM_QUIT||msg.message==WM_CLOSE){puts("VULKAN_PROBE_CANCELLED");goto done;}
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    if(!IsWindow(hwnd)||IsIconic(hwnd)){puts("VULKAN_PROBE_CANCELLED window unavailable");goto done;}
    /* Prior fence was waited before reusing the mapped staging buffer and
       acquire semaphore. Present-complete semaphore is per swapchain image,
       reused only after reacquiring that same image. */
    for(uint32_t y=0;y<H;y++)for(uint32_t x=0;x<W;x++){
        uint16_t p=(uint16_t)(((((x+frame*3)%W)*31/(W-1))<<11)|((y*63/(H-1))<<5)|((((x+frame*2)/20+y/20)&1)*31));
        pixels[y*W+x]=rgba565(p);
    }
    if(frame){CHECK(vkResetFences(device,1,&fence));CHECK(vkResetCommandBuffer(cmd,0));}
    uint32_t index=0;VkResult ar=vkAcquireNextImageKHR(device,swap,5000000000ull,acquired,VK_NULL_HANDLE,&index);
    if((ar!=VK_SUCCESS&&ar!=VK_SUBOPTIMAL_KHR)||index>=count)goto done;
    VkCommandBufferBeginInfo cb={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHECK(vkBeginCommandBuffer(cmd,&cb));
    VkImageSubresourceRange range={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    VkImageMemoryBarrier barrier={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.subresourceRange=range;
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.image=source;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    VkBufferImageCopy copy={0};copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;copy.imageSubresource.layerCount=1;
    copy.imageExtent=ic.extent;vkCmdCopyBufferToImage(cmd,buffer,source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
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
    vkCmdBlitImage(cmd,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
    barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;barrier.dstAccessMask=0;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);
    CHECK(vkEndCommandBuffer(cmd));
    VkPipelineStageFlags wait_stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit={VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired;
    submit.pWaitDstStageMask=&wait_stage;submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
    submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&finished[index];
    CHECK(vkQueueSubmit(queue,1,&submit,fence));
    CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ull));
    VkPresentInfoKHR present={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount=1;present.pWaitSemaphores=&finished[index];present.swapchainCount=1;present.pSwapchains=&swap;present.pImageIndices=&index;
    VkResult pr=vkQueuePresentKHR(queue,&present);
    if(pr!=VK_SUCCESS&&pr!=VK_SUBOPTIMAL_KHR){fprintf(stderr,"vkQueuePresentKHR: %d\n",(int)pr);goto done;}
    submitted++;
    }
    CHECK(wait_idle(device));
    QueryPerformanceCounter(&t1);
    double ms=1000.0*(double)(t1.QuadPart-t0.QuadPart)/(double)freq.QuadPart;
    printf("VULKAN_PROBE_PRESENT_PASS source=640x400 viewport=%ux%u offset=%u,%u filter=linear frames=%u elapsed_ms=%.2f mean_ms=%.3f (FIFO includes refresh wait)\n",v.w,v.h,v.x,v.y,submitted,ms,ms/submitted);
    rc=0;
done:
    if(device&&gdpa){
        if(wait_idle)wait_idle(device);
        #define DESTROY(fn,obj) do{if(obj){PFN_##fn f=(PFN_##fn)gdpa(device,#fn);if(f)f(device,obj,NULL);}}while(0)
        if(mapped&&unmap_memory)unmap_memory(device,bm);
        DESTROY(vkDestroyFence,fence);for(uint32_t i=0;i<finish_count;i++){DESTROY(vkDestroySemaphore,finished[i]);}DESTROY(vkDestroySemaphore,acquired);
        DESTROY(vkDestroyCommandPool,pool);DESTROY(vkDestroyImage,source);DESTROY(vkDestroyBuffer,buffer);
        DESTROY(vkFreeMemory,im);DESTROY(vkFreeMemory,bm);DESTROY(vkDestroySwapchainKHR,swap);
        if(destroy_device)destroy_device(device,NULL);
    }
    if(surface&&destroy_surface)destroy_surface(instance,surface,NULL);
    if(hwnd)DestroyWindow(hwnd);
    if(instance&&destroy_instance)destroy_instance(instance,NULL);
    FreeLibrary(dll);
    if(rc==77)puts("VULKAN_PROBE_SKIPPED (no suitable runtime/device)");
    return rc;
}
int main(int argc,char **argv){
    if(selftest())return 1;
    if(argc==2&&!strcmp(argv[1],"--self-test"))return 0;
    /* A separate log allows double-click testing without losing console output. */
    FILE *out=NULL,*err=NULL;
    if(freopen_s(&out,"VulkanProbe.log","w",stdout)!=0)return 1;
    if(freopen_s(&err,"VulkanProbe-error.log","w",stderr)!=0)return 1;
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    puts("MatroxVulkanProbe v2 / 180-frame synthetic presentation");
    int rc=probe();printf("VULKAN_PROBE_EXIT=%d\n",rc);return rc;
}
