/* Synthetic native Windows x86 edge coverage test for optional MSI95
 * top-left rule. No game binaries/textures or original Matrox DLL. */
typedef unsigned int DWORD;
int _fltused = 0; /* freestanding MSVC x86 floating point */
#include "msi_edge_top_left.h"
__declspec(dllimport) void __stdcall ExitProcess(DWORD);
static float cross(float ax,float ay,float bx,float by){
    return ax*by-ay*bx;
}
static int add_triangle(const float a[2],const float b[2],
        const float c[2],int counts[4],int legacy[4]){
    float den=cross(b[0]-a[0],b[1]-a[1],c[0]-a[0],c[1]-a[1]);
    if(den>-0.0001f&&den<0.0001f)return 11;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        float px=(float)x+0.5f,py=(float)y+0.5f;
        float w1=cross(px-a[0],py-a[1],c[0]-a[0],c[1]-a[1])/den;
        float w2=cross(b[0]-a[0],b[1]-a[1],px-a[0],py-a[1])/den;
        float w0=1.f-w1-w2;
        int j=y*2+x;
        if(w0>=-0.00001f&&w1>=-0.00001f&&w2>=-0.00001f)
            legacy[j]++;
        if(msi_top_left_accept(w0,w1,w2,a[0],a[1],b[0],b[1],
                               c[0],c[1],den))counts[j]++;
    }
    return 0;
}
static DWORD run_test(void){
    const float a[2]={0.f,0.f},b[2]={2.f,0.f};
    const float c[2]={0.f,2.f},d[2]={2.f,2.f};
    int counts[4]={0,0,0,0},legacy[4]={0,0,0,0};
    if(add_triangle(a,b,c,counts,legacy))return 11;
    if(add_triangle(d,c,b,counts,legacy))return 12;
    for(int i=0;i<4;i++)if(counts[i]!=1)return 21u+(DWORD)i;
    /* Legacy inclusive edges write the two shared diagonal pixels twice;
     * the new rule must remove both duplicates without any cracks. */
    int overdrawn=0;
    for(int i=0;i<4;i++)if(legacy[i]>1)overdrawn++;
    if(overdrawn!=2)return 29;
    int reverse[4]={0,0,0,0},legacy_rev[4]={0,0,0,0};
    if(add_triangle(a,c,b,reverse,legacy_rev))return 31;
    if(add_triangle(d,b,c,reverse,legacy_rev))return 32;
    for(int i=0;i<4;i++)if(reverse[i]!=1)return 41u+(DWORD)i;
    return 0;
}
void mainCRTStartup(void){ExitProcess(run_test());}
