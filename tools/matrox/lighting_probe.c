/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent synthetic MSI95 lighting probe. No game or proprietary code.
 * Win32 x86, freestanding, only legacy Win32 imports. See README for limits.
 */
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef long LONG;
typedef int BOOL;
typedef void *HANDLE;
typedef LONG (__stdcall *WNDPROC)(HANDLE,unsigned,DWORD,LONG);
int _fltused=0;
__declspec(dllimport) HANDLE __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(HANDLE,const char*);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HANDLE);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) BOOL __stdcall DeleteFileA(const char*);
__declspec(dllimport) void __stdcall ExitProcess(unsigned);
__declspec(dllimport) LONG __stdcall DefWindowProcA(HANDLE,unsigned,DWORD,LONG);

/* ABI layout from public MSI95 header, independently expressed. */
typedef struct {
    short major,minor;
    struct { LONG bpp,rd,gd,bd,ad,rs,gs,bs,as,pitch; volatile BYTE *mem; LONG size; } color;
    struct { LONG bpp,depth,shift,pitch; volatile BYTE *mem; LONG size; } depth;
    struct { volatile BYTE *mem; LONG size; } texture;
} Info;
typedef struct { float x,y,z,invw,r,g,b,mr,mg,mb,u,v; } Vertex;
typedef struct {
    struct { BOOL enable; LONG width,height,planes; BYTE *mem,*heap; LONG offset;
        struct { BYTE *mem,*heap; LONG offset; } lut;
        BOOL clampu,clampv,modulate,decal,transparency;
        WORD key,mask,alpha,alphamask;
    } texture;
    struct { BOOL enable; DWORD compare; BOOL protect; } depth;
    struct { BOOL dither,protect; } color;
} Params;
typedef Info* (__cdecl *Init)(LONG,LONG,LONG,BOOL,BOOL,WNDPROC);
typedef BOOL (__cdecl *Start)(BOOL,float,float,float,BOOL,float);
typedef BOOL (__cdecl *Set)(Params*);
typedef BOOL (__cdecl *Draw)(Vertex*,Vertex*,Vertex*,BYTE);
typedef BOOL (__cdecl *End)(BOOL,LONG,BOOL);
typedef BOOL (__cdecl *Exit)(void);
typedef BYTE* (__cdecl *Alloc)(LONG);
typedef BOOL (__cdecl *Free)(BYTE*);
typedef BOOL (__cdecl *Blend)(DWORD);
static Init init; static Start start; static Set set; static Draw draw;
static End end; static Exit finish; static Alloc alloc; static Free release;
static Blend blend;
static Params p;
static Vertex v[4];
static BYTE pixels[640u*400u*2u];
static const WORD colors[8]={0xffff,0x8410,0x4208,0x0841,0xf800,0x07e0,0x001f,0x7bef};
static const float levels[16]={0.f,.25f,.5f,1.f,2.f,3.f,4.f,7.f,8.f,15.f,16.f,31.f,63.f,127.f,254.f,255.f};
static LONG __stdcall callback(HANDLE w,unsigned m,DWORD a,LONG b){return DefWindowProcA(w,m,a,b);}
static void vertex(Vertex *q,float x,float y,float u,float light){
    q->x=x;q->y=y;q->z=.4f;q->invw=1.f;
    q->r=q->g=q->b=255.f;q->mr=q->mg=q->mb=light;
    q->u=u;q->v=.5f;
}
static BOOL rect(float x,float y,float w,float h,float u,float left,float right){
    vertex(v+0,x,y,u,left);vertex(v+1,x+w,y,u,right);
    vertex(v+2,x+w,y+h,u,right);vertex(v+3,x,y+h,u,left);
    return draw(v+0,v+1,v+2,100)&&draw(v+0,v+2,v+3,100);
}
static BOOL write_exact(HANDLE h,const void *data,DWORD size){
    DWORD written=0;return WriteFile(h,data,size,&written,0)&&written==size;
}
static void le32(BYTE *p,DWORD n){p[0]=(BYTE)n;p[1]=(BYTE)(n>>8);p[2]=(BYTE)(n>>16);p[3]=(BYTE)(n>>24);}
static BOOL save(const char *name){
    static BYTE header[66];
    HANDLE h=CreateFileA(name,0x40000000ul,0,0,1,0x80,0); /* CREATE_NEW */
    BOOL ok;
    if(h==(HANDLE)-1)return 0;
    header[0]='B';header[1]='M';
    le32(header+2,66+sizeof(pixels));le32(header+10,66);le32(header+14,40);
    le32(header+18,640);le32(header+22,(DWORD)-400);
    header[26]=1;header[28]=16;le32(header+30,3);le32(header+34,sizeof(pixels));
    le32(header+54,0xf800);le32(header+58,0x07e0);le32(header+62,0x001f);
    ok=write_exact(h,header,sizeof(header))&&write_exact(h,pixels,sizeof(pixels));
    if(!CloseHandle(h))ok=0;
    if(!ok)DeleteFileA(name);
    return ok;
}
void __cdecl probe_main(void){
    HANDLE lib=LoadLibraryA(".\\msi95.dll");Info *info=0;BYTE *tex=0,*lut=0;
    unsigned rc=1,i,x,y,d,row,col,mode;
    const char *status; HANDLE report;
    if(!lib)ExitProcess(1);
    init=(Init)GetProcAddress(lib,"msiInit");start=(Start)GetProcAddress(lib,"msiStartFrame");
    set=(Set)GetProcAddress(lib,"msiSetParameters");draw=(Draw)GetProcAddress(lib,"msiRenderTriangle");
    end=(End)GetProcAddress(lib,"msiEndFrame");finish=(Exit)GetProcAddress(lib,"msiExit");
    alloc=(Alloc)GetProcAddress(lib,"msiAllocTextureHeap");release=(Free)GetProcAddress(lib,"msiFreeTextureHeap");
    blend=(Blend)GetProcAddress(lib,"msiSetTextureBlend");
    rc=2;if(!init||!start||!set||!draw||!end||!finish||!alloc||!release)goto done;
    rc=3;info=init(640,400,16,0,0,callback);if(!info)goto done;
    rc=4;if(!start(1,80.f,80.f,80.f,0,0.f))goto done;
    rc=5;tex=alloc(1);lut=alloc(1);if(!tex||!lut)goto done;
    for(y=0;y<64;y++)for(x=0;x<64;x++)tex[y*64+x]=(BYTE)(1+x/8);
    for(i=0;i<512;i++)lut[i]=0;
    for(i=0;i<8;i++){lut[2*(i+1)]=(BYTE)colors[i];lut[2*(i+1)+1]=(BYTE)(colors[i]>>8);}
    for(mode=0;mode<(blend?2u:1u);mode++){
    if(mode){rc=4;if(!start(1,80.f,80.f,80.f,0,0.f))goto done;}
    p.texture.enable=1;p.texture.width=p.texture.height=64;p.texture.planes=8;
    p.texture.mem=p.texture.heap=tex;p.texture.offset=0;
    p.texture.lut.mem=p.texture.lut.heap=lut;p.texture.lut.offset=4096;
    p.texture.clampu=p.texture.clampv=1;p.texture.modulate=1;
    p.texture.mask=0xffff;p.texture.alpha=1;p.depth.compare=4;
    rc=6;
    for(d=0;d<2;d++){
        p.color.dither=(BOOL)d;if(!set(&p))goto done;
        if(mode && !blend(2u)){rc=11;goto done;} /* SDK MODULATE */
        /* Eight source colors x sixteen constant lighting coefficients. */
        for(row=0;row<8;row++)for(col=0;col<16;col++){
            if(!rect((float)(16+38*col),(float)(8+22*(row+8*d)),32.f,18.f,
                     ((float)row+.5f)/8.f,levels[col],levels[col]))goto done;
        }
        /* White-texture Gouraud ramp, 0..255 across 608 native pixels. */
        if(!rect(16.f,(float)(362+16*d),608.f,12.f,.0625f,0.f,255.f))goto done;
    }
    rc=7;if(!set(0))goto done; /* SDK direct framebuffer access / synchronization */
    if(info->color.bpp!=2||info->color.rd!=5||info->color.gd!=6||info->color.bd!=5||
       info->color.rs!=11||info->color.gs!=5||info->color.bs!=0||
       !info->color.mem||info->color.pitch<1280||info->color.pitch>65536||
       info->color.size<info->color.pitch*400)goto done;
    for(y=0;y<400;y++)for(x=0;x<1280;x++)
        pixels[y*1280+x]=info->color.mem[y*(DWORD)info->color.pitch+x];
    rc=8;p.texture.enable=0;p.texture.mem=p.texture.heap=0;p.texture.lut.mem=p.texture.lut.heap=0;
    if(!set(&p)||!end(0,0,1))goto done;
    rc=9;if(!save(mode?"msi95_lighting_blend.bmp":"msi95_lighting_flag.bmp"))goto done;
    }
    rc=12;
    status=blend?"msiSetTextureBlend=present; MODULATE calls succeeded\r\n":"msiSetTextureBlend=absent; flag-only frame generated\r\n";
    report=CreateFileA("msi95_lighting_probe.txt",0x40000000ul,0,0,1,0x80,0);
    if(report==(HANDLE)-1)goto done;
    for(i=0;status[i];i++){}
    {BOOL ok=write_exact(report,status,i);if(!CloseHandle(report))ok=0;
     if(!ok){DeleteFileA("msi95_lighting_probe.txt");goto done;}}
    rc=0;
done:
    if(info){if(lut&&!release(lut)&&!rc)rc=10;if(tex&&!release(tex)&&!rc)rc=10;if(!finish()&&!rc)rc=10;}
    FreeLibrary(lib);ExitProcess(rc);
}
