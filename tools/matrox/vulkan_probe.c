/* SPDX-License-Identifier: GPL-3.0-or-later
 * Synthetic Win32 test of the reusable Vulkan presenter.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "vulkan_present.h"
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

#define W 640u
#define H 400u
static uint16_t native_pixels[W*H];
static void report(const char *message,int code){printf("%s code=%d\n",message,code);}
static int probe(void){
    HWND hwnd=CreateWindowExW(0,L"STATIC",L"Matrox Vulkan synthetic presentation",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,960,720,NULL,NULL,GetModuleHandleW(NULL),NULL);
    if(!hwnd)return 1;
    ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);
    int rc=matrox_vk_open(hwnd,1,report);
    if(rc){DestroyWindow(hwnd);return rc;}
    LARGE_INTEGER freq,t0,t1;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&t0);
    unsigned submitted=0;
    for(unsigned frame=0;frame<180;frame++){
        MSG msg;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        for(uint32_t y=0;y<H;y++)for(uint32_t x=0;x<W;x++)
            native_pixels[y*W+x]=(uint16_t)(((((x+frame*3)%W)*31/(W-1))<<11)|((y*63/(H-1))<<5)|((((x+frame*2)/20+y/20)&1)*31));
        if(matrox_vk_present(native_pixels,W*2)){rc=1;break;}
        submitted++;
    }
    matrox_vk_close();
    QueryPerformanceCounter(&t1);
    if(!rc){
        RECT r;GetClientRect(hwnd,&r);View v=viewport((uint32_t)r.right,(uint32_t)r.bottom);
        double ms=1000.0*(double)(t1.QuadPart-t0.QuadPart)/(double)freq.QuadPart;
        printf("VULKAN_PROBE_PRESENT_PASS source=640x400 viewport=%ux%u offset=%u,%u filter=linear frames=%u elapsed_ms=%.2f mean_ms=%.3f (FIFO includes refresh wait)\n",v.w,v.h,v.x,v.y,submitted,ms,ms/submitted);
    }
    if(IsWindow(hwnd))DestroyWindow(hwnd);
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
    puts("MatroxVulkanProbe v3 / shared presenter");
    int rc=probe();printf("VULKAN_PROBE_EXIT=%d\n",rc);return rc;
}
