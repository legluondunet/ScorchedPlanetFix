/* No proprietary assets or runtime dependencies. Simulates the optional
 * flat Gouraud post-interpolation RGB565 quantization contract. */
typedef unsigned int DWORD;
#ifdef _WIN32
__declspec(dllimport) void __stdcall ExitProcess(DWORD);
#endif
#include "msi_dither_gouraud.h"
static unsigned legacy565(unsigned r,unsigned g,unsigned b){
    return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}
static DWORD run_test(void){
    /* Reproducible, 4x4 tiled, no overflow at channel extrema. */
    for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
        if(msi_gouraud_ordered_565(0,0,0,x,y)!=0u)return 1u;
        if(msi_gouraud_ordered_565(255,255,255,x,y)!=0xffffu)return 2u;
        if(msi_gouraud_ordered_565(101,161,207,x,y)!=
           msi_gouraud_ordered_565(101,161,207,x+4u,y+4u))return 3u;
    }
    /* Uniform grayscale must vary spatially (unlike truncation), and keep
     * average quantization within one output step of the input intensity. */
    unsigned changed=0,redsum=0,greensum=0,bluesum=0;
    const unsigned r=123u,g=147u,b=171u;
    for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
        unsigned c=msi_gouraud_ordered_565(r,g,b,x,y);
        if(c!=legacy565(r,g,b))changed++;
        redsum+=(c>>11)&31u;greensum+=(c>>5)&63u;bluesum+=c&31u;
    }
    if(changed==0u||changed==16u)return 4u;
    /* Over 16 pixels Bayer approximates 16 * (source/255 * max). */
    if(redsum+16u < 16u*r*31u/255u||redsum > 16u*r*31u/255u+16u)return 5u;
    if(greensum+16u < 16u*g*63u/255u||greensum > 16u*g*63u/255u+16u)return 6u;
    if(bluesum+16u < 16u*b*31u/255u||bluesum > 16u*b*31u/255u+16u)return 7u;
    /* The historic 0x0841 shadow-color path is identified using its
     * unchanged pre-dither RGB565 value and bypasses this new function. */
    if(legacy565(8,8,8)!=0x0841u)return 8u;
    return 0;
}
#ifdef _WIN32
void mainCRTStartup(void){ExitProcess(run_test());}
#else
int main(void){return (int)run_test();}
#endif
