/* Synthetic native Windows x86 edge coverage test for optional MSI95
 * top-left rule. No game binaries/textures or original Matrox DLL. */
typedef unsigned int DWORD;
int _fltused = 0; /* freestanding MSVC x86 floating point */
#include "msi_edge_top_left.h"
#ifdef _WIN32
__declspec(dllimport) void __stdcall ExitProcess(DWORD);
#endif
#define GRID_W 10
#define GRID_H 10
#define PIXELS (GRID_W * GRID_H)
static float cross(float ax,float ay,float bx,float by){
    return ax*by-ay*bx;
}
static int add_triangle(const float a[2],const float b[2],
        const float c[2],int counts[PIXELS],int *legacy){
    float den=cross(b[0]-a[0],b[1]-a[1],c[0]-a[0],c[1]-a[1]);
    if(den>-0.0001f&&den<0.0001f)return 11;
    for(int y=0;y<GRID_H;y++)for(int x=0;x<GRID_W;x++){
        float px=(float)x+0.5f,py=(float)y+0.5f;
        float w1=cross(px-a[0],py-a[1],c[0]-a[0],c[1]-a[1])/den;
        float w2=cross(b[0]-a[0],b[1]-a[1],px-a[0],py-a[1])/den;
        float w0=1.f-w1-w2;
        int j=y*GRID_W+x;
        if(legacy && w0>=-0.00001f&&w1>=-0.00001f&&w2>=-0.00001f)
            legacy[j]++;
        if(msi_top_left_accept(w0,w1,w2,a[0],a[1],b[0],b[1],
                               c[0],c[1],den))counts[j]++;
    }
    return 0;
}
static void zero_grid(int out[PIXELS]){
    for(int j=0;j<PIXELS;j++)out[j]=0;
}
/* Draw two tessellations of the same quadrilateral, with both winding
 * orders. Results should cover a mathematically half-open rectangle. */
static int rect_case(float left,float top,float right,float bottom,
                     int diagonal,int reverse,int counts[PIXELS],int *legacy){
    const float a[2]={left,top},b[2]={right,top};
    const float c[2]={left,bottom},d[2]={right,bottom};
    const float *v0,*v1,*v2,*u0,*u1,*u2;
    if(diagonal==0){v0=a;v1=b;v2=c;u0=d;u1=c;u2=b;}
    else{v0=a;v1=b;v2=d;u0=a;u1=d;u2=c;}
    if(reverse){const float *tmp=v1;v1=v2;v2=tmp;tmp=u1;u1=u2;u2=tmp;}
    if(add_triangle(v0,v1,v2,counts,legacy))return 1;
    if(add_triangle(u0,u1,u2,counts,legacy))return 2;
    return 0;
}
static int verify_rect(float left,float top,float right,float bottom,
                       int diagonal,int reverse){
    int counts[PIXELS],legacy[PIXELS];
    zero_grid(counts);zero_grid(legacy);
    if(rect_case(left,top,right,bottom,diagonal,reverse,counts,legacy))return 10;
    for(int y=0;y<GRID_H;y++)for(int x=0;x<GRID_W;x++){
        const float px=(float)x+0.5f,py=(float)y+0.5f;
        const int inside=px>=left&&px<right&&py>=top&&py<bottom;
        if(counts[y*GRID_W+x]!=inside)return 21+y*GRID_W+x;
    }
    return 0;
}
static DWORD run_test(void){
    int counts[PIXELS],legacy[PIXELS];
    zero_grid(counts);zero_grid(legacy);
    if(rect_case(0.f,0.f,2.f,2.f,0,0,counts,legacy))return 11;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++)
        if(counts[y*GRID_W+x]!=1)return 21u+(DWORD)(y*2+x);
    int overdrawn=0;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++)
        if(legacy[y*GRID_W+x]>1)overdrawn++;
    if(overdrawn!=2)return 29;
    /* Two triangulations, both windings, integer/fractional and negative
     * origins, plus exact half-pixel boundaries. This catches shared-edge
     * double writes, cracks, and accidental loss of left/top pixels. */
    const float rects[][4]={
        {1.f,1.f,7.f,6.f},
        {1.25f,1.25f,7.25f,6.25f},
        {1.5f,1.5f,7.5f,6.5f},
        {-2.5f,-1.5f,4.5f,3.5f},
        {0.f,0.f,9.f,9.f},
    };
    for(int k=0;k<5;k++)for(int d=0;d<2;d++)for(int w=0;w<2;w++){
        int status=verify_rect(rects[k][0],rects[k][1],rects[k][2],rects[k][3],d,w);
        if(status)return 1000u+(DWORD)(k*4+d*2+w)*200u+(DWORD)status;
    }
    /* Adjacent quads share a vertical edge. A pixel on that edge belongs
     * to the right quad, never to both; no visible seam is permitted. */
    for(int d=0;d<2;d++)for(int w=0;w<2;w++){
        zero_grid(counts);
        if(rect_case(1.5f,1.5f,4.5f,6.5f,d,w,counts,0))return 71;
        if(rect_case(4.5f,1.5f,7.5f,6.5f,d,w,counts,0))return 72;
        for(int y=0;y<GRID_H;y++)for(int x=0;x<GRID_W;x++){
            float px=(float)x+0.5f,py=(float)y+0.5f;
            int inside=px>=1.5f&&px<7.5f&&py>=1.5f&&py<6.5f;
            if(counts[y*GRID_W+x]!=inside)return 3000u+(DWORD)(y*GRID_W+x);
        }
    }
    return 0;
}
#ifdef _WIN32
void mainCRTStartup(void){ExitProcess(run_test());}
#else
int main(void){return (int)run_test();}
#endif
