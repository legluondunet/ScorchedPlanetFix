/* Scorched Planet Matrox Mystique — MSI95 *diagnostic* compatibility shim.
 * Stage 0.36: shared INI-configurable frame ceiling at MSI frame boundary; preserve validated effects.
 * Target: 32-bit x86 PE DLL, cdecl exports (1996 MSI95.dll ABI).
 * No proprietary code or game data included.
 * Compile with clang targeting i686-pc-windows-msvc and lld-link; no CRT.
 */
int _fltused = 0; /* MSVC x86 floating-point support marker; no CRT */
#include "msi_world_bilinear.h"
#include "vulkan_present.h"
#include "msi_gpu_draw_batch.h"
#include "msi_gpu_shadow_sampler.h"
#include "msi_gpu_publish.h"
#include "msi_gpu_coverage.h"
#include "msi_gpu_cpu_replay.h"
#include "msi_gpu_defer_audit.h"
#include "msi_gpu_defer_admit.h"
#include "msi_gpu_deferred_finish.h"
#include "msi_gpu_bypass.h"
#include "msi_gpu_pilot_storage.h"
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int BOOL;
typedef int LONG;
typedef unsigned int UINT;
typedef void * HANDLE;
typedef void * LPVOID;
typedef unsigned char * LPBYTE;
typedef void * WNDPROC;
typedef unsigned long SIZE_T;
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
#define FILE_SHARE_READ 1U
#define FILE_SHARE_WRITE 2U
#define GENERIC_WRITE 0x40000000U
#define CREATE_ALWAYS 2U
#define CREATE_NEW 1U
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#define FILE_ATTRIBUTE_NORMAL 0x80U
#define MEM_COMMIT 0x1000U
#define MEM_RESERVE 0x2000U
#define MEM_RELEASE 0x8000U
#define PAGE_READWRITE 4U
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_ACCESS_VIOLATION 0xC0000005u
#define PAGE_NOACCESS 0x01u
#define PAGE_GUARD 0x100u
#define WM_CREATE 0x0001u
#define WM_ACTIVATE 0x0006u
#define WM_SETFOCUS 0x0007u
#define WM_KILLFOCUS 0x0008u
#define WM_KEYDOWN 0x0100u
#define WM_KEYUP 0x0101u
#define WM_CHAR 0x0102u
#define WM_SYSKEYDOWN 0x0104u
#define WM_SYSKEYUP 0x0105u
#define WM_MOUSEMOVE 0x0200u
#define WM_LBUTTONDOWN 0x0201u
#define WM_LBUTTONUP 0x0202u
#define WM_RBUTTONDOWN 0x0204u
#define WM_RBUTTONUP 0x0205u
#define WM_CLOSE 0x0010u
#define WM_ACTIVATEAPP 0x001Cu
#define WM_NCACTIVATE 0x0086u
#define WM_SETICON 0x0080u
#define ICON_SMALL 0u
#define ICON_BIG 1u
#define IMAGE_ICON 1u
#define LR_SHARED 0x8000u
#define OEM_GROUP_ICON_ID 101u
#define WM_DESTROY 0x0002u
#define WM_PAINT 0x000Fu
#define WM_ERASEBKGND 0x0014u
#define WS_OVERLAPPEDWINDOW 0x00CF0000u
#define WS_VISIBLE 0x10000000u
#define WS_POPUP 0x80000000u
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define BLACKNESS 0x00000042u
#define PM_REMOVE 0x0001u
#define BI_BITFIELDS 3u
#define DIB_RGB_COLORS 0u
#define SRCCOPY 0x00CC0020u
#define CW_USEDEFAULT ((int)0x80000000u)
#define GDI_ERROR 0xFFFFFFFFu
#define HALFTONE 4

__declspec(dllimport) HANDLE __stdcall CreateFileA(const char*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(void*,char*,DWORD);
__declspec(dllimport) DWORD __stdcall GetFileAttributesA(const char*);
__declspec(dllimport) DWORD __stdcall GetLastError(void);
__declspec(dllimport) DWORD __stdcall GetTickCount(void);
/* Monotonic Windows performance counter is available on Windows XP+ and Wine.
 * Keep timing at MSI end-of-frame so the original OEM EXE and resources stay
 * byte-for-byte unchanged. Never tie elapsed-time simulation to target FPS. */
typedef long long MSI_INT64;
typedef struct { MSI_INT64 QuadPart; } MSI_LARGE_INTEGER;
__declspec(dllimport) BOOL __stdcall QueryPerformanceCounter(MSI_LARGE_INTEGER*);
__declspec(dllimport) BOOL __stdcall QueryPerformanceFrequency(MSI_LARGE_INTEGER*);
__declspec(dllimport) void __stdcall Sleep(DWORD);
__declspec(dllimport) DWORD __stdcall GetEnvironmentVariableA(const char*,char*,DWORD);
__declspec(dllimport) HANDLE __stdcall GetModuleHandleA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(HANDLE,const char*);
__declspec(dllimport) void * __stdcall VirtualAlloc(void*,DWORD,DWORD,DWORD);
__declspec(dllimport) BOOL __stdcall VirtualFree(void*,DWORD,DWORD);
__declspec(dllimport) HANDLE __stdcall CreateFileMappingA(HANDLE,void*,DWORD,DWORD,DWORD,const char*);
__declspec(dllimport) void* __stdcall MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,SIZE_T);
__declspec(dllimport) BOOL __stdcall UnmapViewOfFile(const void*);
__declspec(dllimport) DWORD __stdcall GetCurrentThreadId(void);
__declspec(dllimport) void* __stdcall AddVectoredExceptionHandler(unsigned long, void*);
__declspec(dllimport) unsigned long __stdcall VirtualQuery(const void*,void*,SIZE_T);

/* Minimal Windows x86 ABI definitions for diagnostic GDI presentation.
 * Created from msiInit (never DllMain). The preview has its own WndProc,
 * independent of the game's original Win95 callbacks and message handling. */
typedef void* HWND;
typedef void* HDC;
typedef unsigned long WPARAM;
typedef long LPARAM;
typedef long LRESULT;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x,y; } POINT;
typedef LRESULT (__stdcall *WINWNDPROC)(HWND,UINT,WPARAM,LPARAM);
typedef struct { UINT style; WINWNDPROC lpfnWndProc; LONG cbClsExtra,cbWndExtra;
    void* hInstance,*hIcon,*hCursor,*hbrBackground;
    const char* lpszMenuName,*lpszClassName; } WNDCLASSA_MIN;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG_MIN;
typedef struct { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore,fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT_MIN;
typedef struct { DWORD biSize; LONG biWidth,biHeight; WORD biPlanes,biBitCount;
    DWORD biCompression,biSizeImage; LONG biXPelsPerMeter,biYPelsPerMeter;
    DWORD biClrUsed,biClrImportant; } BITMAPINFOHEADER_MIN;
typedef struct { BITMAPINFOHEADER_MIN bmi; DWORD masks[3]; } BITMAPINFO565;
__declspec(dllimport) WORD __stdcall RegisterClassA(const WNDCLASSA_MIN*);
__declspec(dllimport) HWND __stdcall CreateWindowExA(DWORD,const char*,const char*,DWORD,int,int,int,int,HWND,void*,void*,void*);
__declspec(dllimport) BOOL __stdcall ShowWindow(HWND,int);
__declspec(dllimport) BOOL __stdcall UpdateWindow(HWND);
__declspec(dllimport) HANDLE __stdcall LoadImageA(void*,const char*,UINT,int,int,UINT);
__declspec(dllimport) LRESULT __stdcall SendMessageA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) BOOL __stdcall PeekMessageA(MSG_MIN*,HWND,UINT,UINT,UINT);
__declspec(dllimport) BOOL __stdcall TranslateMessage(const MSG_MIN*);
__declspec(dllimport) LRESULT __stdcall DispatchMessageA(const MSG_MIN*);
__declspec(dllimport) LRESULT __stdcall DefWindowProcA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) LRESULT __stdcall CallWindowProcA(WINWNDPROC,HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) HDC __stdcall GetDC(HWND);
__declspec(dllimport) int __stdcall ReleaseDC(HWND,HDC);
__declspec(dllimport) BOOL __stdcall GetClientRect(HWND,RECT*);
__declspec(dllimport) BOOL __stdcall AdjustWindowRect(RECT*,DWORD,BOOL);
__declspec(dllimport) int __stdcall GetSystemMetrics(int);
__declspec(dllimport) BOOL __stdcall DestroyWindow(HWND);
__declspec(dllimport) HDC __stdcall BeginPaint(HWND,PAINTSTRUCT_MIN*);
__declspec(dllimport) BOOL __stdcall EndPaint(HWND,const PAINTSTRUCT_MIN*);
__declspec(dllimport) int __stdcall StretchDIBits(HDC,int,int,int,int,int,int,int,int,const void*,const BITMAPINFO565*,UINT,DWORD);
__declspec(dllimport) BOOL __stdcall PatBlt(HDC,int,int,int,int,DWORD);
__declspec(dllimport) int __stdcall SetStretchBltMode(HDC,int);
__declspec(dllimport) BOOL __stdcall SetBrushOrgEx(HDC,int,int,POINT*);


/* Matches MSI95/msi95.h in official Matrox SDK; all 32-bit layout. */
typedef struct {
    short VersionMajor, VersionMinor;
    struct { LONG BytesPerPixel, rDepth,gDepth,bDepth,aDepth,rBitShift,gBitShift,bBitShift,aBitShift,Pitch; volatile LPBYTE pMem; LONG MemSize; } msiColor;
    struct { LONG BytesPerPixel,zDepth,zBitShift,Pitch; volatile LPBYTE pMem; LONG MemSize; } msiDepth;
    struct { volatile LPBYTE pMem; LONG MemSize; } msiTexture;
} T_msiInfo;
typedef struct { float x,y,z,invW,r,g,b,mr,mg,mb,u,v; } T_msiVertex;
typedef struct {
    struct { BOOL Enable; LONG Width,Height,Planes; LPBYTE pMem,pHeap; LONG CacheOffset;
        struct { LPBYTE pMem,pHeap; LONG CacheOffset; } msiLUT;
        BOOL Clamp_u,Clamp_v,Modulate,Decal,Transparency;
        WORD KeyingColor,KeyingColorMask,KeyingAlpha,KeyingAlphaMask; } msiTexture;
    struct { BOOL Enable; DWORD Compare; BOOL Protect; } msiDepth;
    struct { BOOL Dither; BOOL Protect; } msiColor;
} T_msiParameters;
typedef struct { LONG x,y,u,v,xWidth,yHeight,uWidth,vHeight; float z,r,g,b,mr,mg,mb; } T_msiZPlane;

static void *g_module = NULL;
static HANDLE g_log = NULL;
static unsigned g_frames = 0, g_triangles = 0, g_zplanes = 0, g_params = 0;
static unsigned g_zplane_draw_context=0, g_zplane_blue_key_skips=0;
/* Exact raw BYTE opacity requests at the public API boundary, not internal
 * rasterizer calls or ShadowOnce's synthetic opacity. Reset per msiInit.
 * Counters saturate rather than wrap; a saturated bin is explicitly marked. */
static unsigned g_triangle_opacity[256],g_zplane_opacity[256];
static void count_opacity(unsigned *bins,BYTE opacity){
    if(bins[opacity]!=0xFFFFFFFFu)++bins[opacity];
}
/* Number of Win32 region checks reused for additional triangle vertices.
 * No caching across triangles/frames: every call still queries fresh memory. */
static unsigned g_vertex_region_reuses=0, g_vertex_region_fallbacks=0;
/* Large HUD atlases can be only partially populated. Older probes required
 * both extreme texture-cache pages to be uploaded, rejecting *all* pixels
 * of a 512x512 / 1024x1024 image when only a small region was resident.
 * Track subtexel residency so missing regions stay transparent. */
static unsigned g_sparse_atlas_triangles=0;
static unsigned g_sparse_atlas_drawn_pixels=0;
static unsigned g_sparse_atlas_unmapped_pixels=0;
static unsigned g_sparse_atlas_direct_pixels=0;
/* Read-only attribution for the existing sparse-atlas zero skip: count
 * how often zero WOULD be transparent according to the currently bound
 * palette and original color key, vs potentially opaque palette zero.
 * Neither class changes the current renderer's skip decision. */
static unsigned g_sparse_atlas_zero_keyed_skips=0;
static unsigned g_sparse_atlas_zero_unkeyed_skips=0;
static unsigned g_sparse_atlas_log_count=0;
/* v0.20: capture per-frame ownership for selected rendered frames only.
 * 1: world textured nonblack, 2: world textured black, 3: world flat nonblack,
 * 4: world flat black, 5: Z-plane nonblack, 6: Z-plane black.
 * 7: world key-transparent with no earlier MSI write, 8: Z-plane keyed,
 * 9: Z-plane sparse-atlas sample missing; these NEVER alter game pixels.
 * Zero means NO MSI pixel write in this frame, not necessarily empty FB:
 * game CPU writes or previous framebuffer content can still be present.
 * Diagnostic buffer is NEVER used as input to the main rasterizer. */
static BYTE *g_ownership = NULL;
/* F12-only provenance of the last attempted sparse Z-plane atlas sample.
 * 0=not sampled, 1=uploaded nonzero, 2=uploaded zero, 3=unmarked nonzero,
 * 4=unmarked zero (heuristically skipped), 5=keyed transparency.
 * Zero index is NOT evidence of a missing asset; diagnostic only. */
static BYTE *g_atlas_provenance=NULL;
/* F12-only last ordinary world-texture sample, never used for rendering. */
static BYTE *g_world_provenance=NULL;
/* F12 only: final MSI shadow-write origin; never read by the rasterizer.
 * 1=blended with textured base, 2=opaque without textured base,
 * 3=other dark-flat with base, 4=other dark-flat without base,
 * 5=unexpected prior-blend sentinel. 0=other final MSI writer. */
static BYTE *g_shadow_provenance=NULL;
/* F12-only FINAL successful MSI world-texture source class, overwritten by later
 * MSI writes. 0=other final writer, 1=index1 black, 2=index241 black,
 * 3=index0 black, 4=other 8bpp black, 5=other 8bpp nonblack,
 * 6=16bpp black, 7=16bpp nonblack, 8=4bpp black, 9=4bpp nonblack,
 * 10=index1 nonblack, 11=index241 nonblack. Never read by rasterization. */
static BYTE *g_final_texel_source=NULL;
/* F12-only final successful world texture source: packed, lossless 16-bit
 * diagnostic value, written only beside the actual framebuffer write.
 * bit14=8bpp, bit13=16bpp, bit12=4bpp; low8=EXACT raw 8bpp palette index;
 * bit8=pre-lighting texture sample RGB565 was zero; bit9=modulation active;
 * bit10=ordered dither active; bit11=alpha blended with prior framebuffer.
 * Zero=not an ordinary final world-texture write. This must NEVER influence
 * the rasterizer, palette keying, colors, or any game pixels. */
static WORD *g_final_source_audit=NULL;
/* One-frame, F12-only "same last MSI writer without ordered dither" RGB565.
 * For each FINAL dithered world-texture draw, store the existing undithered
 * modulation reference, with the SAME actual framebuffer underlay for alpha
 * blending. Untracked/overwritten/non-dither final writers clear to zero.
 * The _sourceaudit.bmp bit10 identifies where zero is a valid reference.
 * Never read this array when rasterizing or presenting actual game pixels. */
static WORD *g_undithered_reference=NULL;
/* F12-only exact five-byte pre-modulation RGB565 sample + 3x8-bit vertex
 * lighting coefficient of the last successful MSI world texture write.
 * Never read by rasterization. The seventh _sourceaudit marks valid pixels.
 * Non-modulated world textures use neutral modifiers 255,255,255. */
static BYTE *g_final_light_inputs=NULL;
/* F12 only: count original (pre-LUT) 8-bit indices whose resolved RGB565
 * value matched the hardware color key. This is not a pixel-coverage map:
 * overdraw and triangle overlap can count a texel more than once. */
static unsigned g_world_rgbkey_index_hist[256];
/* F12-only CLUT alias analysis, no influence on keying/rasterizer:
 * count ALL original RGB565 key matches BEFORE palette index 1 preview
 * exempts its black source pixel. This includes draws/overdraw, not
 * unique pixels nor proof that a palette entry is used in this frame. */
static unsigned g_world_key_candidate_hist[256];
static unsigned g_world_index1_preview_attempts=0;
static unsigned g_key_alias_bind_reports=0;
static unsigned g_key_alias_seen[24][4]; /* texture offset, CLUT offset, key, mask */

static unsigned g_ownership_bytes = 0;
static int g_capture_ownership = 0;
static unsigned g_world_texture_samples = 0;
/* v0.31: passive audit of MSI95 rendering capabilities actually requested
 * by the OEM game. Matrox Mystique has no bilinear filtering in MSI95.
 * These counters NEVER influence the framebuffer or its ownership maps. */
static unsigned g_fx_param_updates=0;
static unsigned g_fx_dither_parameters=0,g_fx_modulate_parameters=0,g_fx_decal_parameters=0;
static unsigned g_fx_dither_textured_states=0,g_fx_modulate_textured_states=0;
static unsigned g_fx_dither_triangles=0,g_fx_modulate_triangles=0,g_fx_decal_triangles=0;
static unsigned g_fx_dither_zplanes=0,g_fx_modulate_zplanes=0,g_fx_decal_zplanes=0;
/* Passive original MSI95 state audit. This reports *requests* by the
 * OEM client, not proof that each effect was accurately implemented. */
static unsigned g_native_tex_transparency=0,g_native_alpha_mask=0;
static unsigned g_native_tex_clamp_u=0,g_native_tex_clamp_v=0;
static unsigned g_native_depth_enabled=0,g_native_depth_protect=0;
static unsigned g_native_color_protect=0,g_native_state_details=0;
/* v0.33 per-vertex texture-color modulation, independently selectable.
 * v0.34 ordered RGB565 quantization experiment, independently selectable;
 * it affects ONLY opt-in modulated 3D textures with Dither set in MSI95. */
static int g_msi_modulate_enabled=0,g_msi_dither_enabled=0;
static int g_rgb888_lighting_enabled=0;
static unsigned g_modulate_triangles=0,g_modulate_pixels=0,g_modulate_varying=0;
static unsigned g_dither_candidates=0,g_dither_changed=0,g_dither_bypassed=0;
static unsigned g_fx_vertex_color_logs=0,g_fx_param_state_logs=0;
/* FPS cap from [scorch] FPSLimit via launcher environment; default 60.
 * Zero disables pacing. Limits only at EndFrame, not during HUD draws. */
static unsigned g_cap_target_fps=60u;
static DWORD g_cap_frequency=0;
static MSI_INT64 g_cap_last_tick=0;
static DWORD g_cap_last_ms=0;
static unsigned g_cap_waited_frames=0,g_cap_late_frames=0;
static unsigned g_cap_sleep_ms=0,g_cap_fallback_frames=0;
static int g_cap_qpc_ready=0;

/* Experimental perspective correction is off unless explicitly selected
 * by the launcher. It never affects the 2D Z-plane HUD. */
static int g_perspective_enabled=0;
static unsigned g_perspective_triangles=0, g_perspective_pixels=0;
static unsigned g_world_varying_invw=0, g_world_constant_invw=0;
static unsigned g_world_invalid_invw=0;
/* Modern optional rendering experiment: RGB565 bilinear ONLY on opaque
 * 3D world textures whose vertex invW actually varies. The 1996
 * Mystique used nearest sampling; default OFF for original fidelity.
 * Transparent textures, menus (constant invW) and Z-plane HUD bypass. */
static int g_world_bilinear_enabled=0;
static unsigned g_world_bilinear_triangles=0;
static unsigned g_world_bilinear_samples=0,g_world_bilinear_changed=0;
static unsigned g_world_bilinear_keyed_bypass=0;
static unsigned g_world_bilinear_palette_triangles=0;
static unsigned g_world_bilinear_palette_samples=0;
static unsigned g_world_bilinear_uniform_samples=0;
/* Explicit A/B experiment: the game uses RGB565 0x00FF for some blue
 * sprite borders in the 3D world. Honor the original color key always;
 * only opt-in a second exact-blue key on textured transparent world draws.
 * The HUD's existing Z-plane blue-key path remains untouched. */
static int g_world_blue_key_enabled=0;
static unsigned g_world_blue_samples=0, g_world_blue_eligible=0, g_world_blue_skipped=0;
static unsigned g_world_blue_diagnostic_count=0;
/* v0.26: trial of matching an 8-bit source palette index against MSI's
 * KeyingColor before LUT conversion, rather than post-LUT RGB565 only.
 * OFF by default. Restrict to world triangles with Transparency set. */
static int g_index_key_enabled=0;
/* Opt-in authentic-asset experiment. Only original TITLEM icon01 contains
 * 6,993 source-index-241 background texels. Its live cached atlas is
 * identified by full 128x128 byte content, never by a hardcoded address.
 * No original game code or texture is modified. */
static int g_icon01_opaque_enabled=0;
static unsigned g_icon01_cache_offset=0xffffffffu;
static unsigned g_icon01_verified_uploads=0;
static unsigned g_icon01_opaque_samples=0;

static unsigned g_index_key_candidates=0,g_index_key_skipped=0;
static unsigned g_index_key_blue_candidates=0,g_index_key_other_candidates=0;
static unsigned g_index_key_debug=0;
/* v0.23 optional experiment: blend only near-black non-textured 3D
 * geometry (observed RGB565 0x0841 on diagnostic shadow/void polygons).
 * Never modify the normal/reference framebuffer when disabled. */
static int g_shadow_blend_enabled=0;
static unsigned g_shadow_dark_candidates=0,g_shadow_dark_blended=0;
static unsigned g_shadow_diagnostic_count=0;
/* v0.27: optional conservative experiment. Dark flat world pixels receive
 * 50-percent blending only when a textured MSI world pixel has already been
 * written at the same coordinate in the SAME frame. This is not proof of the
 * original Matrox shadow algorithm; it prevents our 0x0841 heuristic from
 * changing unoccluded sky/background. Keep this tracker independent of the
 * per-frame ownership capture and the real framebuffer. */
static int g_shadow_underlay_enabled=0;
static BYTE *g_shadow_underlay=NULL;
static unsigned g_shadow_underlay_pixels=0;
static unsigned g_shadow_underlay_opaque=0;
static unsigned g_shadow_underlay_transparent=0;
static unsigned g_shadow_without_underlay=0;
static unsigned g_shadow_underlay_blended=0;
static unsigned g_shadow_underlay_debug=0;
/* v0.30 diagnostic trial: track shadow application per pixel, per frame.
 * Underlay state 1/2 means textured world paint; state 3 means a dark
 * triangle already blended over that texture. A subsequent dark triangle
 * should not erase the previous blend or blend twice on an overlapping edge.
 * This is still an *experimental* 50pct opacity heuristic, not MSI native
 * alpha. Legacy --shadow-underlay behavior is kept for A/B testing. */
static int g_shadow_once_enabled=0;
static unsigned g_shadow_once_blended=0;
static unsigned g_shadow_once_repeated=0;
static unsigned g_shadow_once_no_base=0;
static unsigned g_shadow_once_nonshadow_resets=0;

/* One manual pair per distinct F12 keypress. A file number is reserved
 * lazily in the game's own directory; previously created images are NEVER
 * overwritten, including after game restarts or older experiments. */
static unsigned g_manual_capture_pending=0,g_manual_capture_active=0;
static unsigned g_manual_capture_sequence=0,g_f12_held=0;
/* v0.24 purely diagnostic. Capture the first significant opaque-blue sprite
 * automatically, plus the next complete frame with ownership, without
 * changing a single game framebuffer pixel or requiring an F12 press. */
static unsigned g_auto_blue_frame_samples=0;
static unsigned g_auto_blue_first_captured=0;
static unsigned g_auto_blue_pair_pending=0;
static unsigned g_auto_blue_pair_captured=0;
/* v0.25: per-frame rejected-blue provenance remains visible even if a later
 * triangle or HUD Z-plane overwrites the same pixel. The two buffers below
 * are diagnostics only; neither is ever sampled by the game renderer. */
static BYTE g_blue_discard_mask[640u*400u];
static unsigned g_blue_mask_frame=0xFFFFFFFFu;
static BYTE g_blue_atlas_texels[65536u];
static BYTE g_blue_atlas_palette[512u];
static unsigned g_blue_atlas_width=0,g_blue_atlas_height=0;
static unsigned g_blue_atlas_planes=0,g_blue_atlas_tex_offset=0,g_blue_atlas_lut_offset=0;
static unsigned g_blue_atlas_key=0,g_blue_atlas_key_mask=0;
static int g_blue_atlas_valid=0;


static T_msiInfo g_info;
static void *g_color, *g_depth, *g_texture;
/* Diagnostic dual view: public guest alias and private engine alias.
 * They refer to the SAME bytes. Both remain read/write in this stage. */
static void *g_color_guest;
static unsigned g_gpu_dual_checks,g_gpu_dual_mismatch,g_gpu_dual_thread_checks,g_gpu_dual_foreign;
static DWORD g_gpu_dual_owner;
/* The original game may keep hundreds of texture heaps live across scene
 * transitions. The previous shim's fixed 256-entry array returned NULL for
 * valid 4-page allocations, corrupting the game's scene data. Track actual
 * VirtualAlloc failures instead of imposing an artificial heap-count cap.
 * A geometrically growing pointer table is independently VirtualAlloc'd. */
static void **g_heaps=NULL;
static unsigned g_heap_count=0,g_heap_capacity=0,g_heap_peak=0;
static int g_initialized = 0;
static unsigned g_snapshot_mask = 0;
static int g_log_attempted = 0;
static int g_veh_registered = 0;
static volatile unsigned g_veh_count = 0;
static HWND g_preview = NULL;
static WINWNDPROC g_guest_window_proc = NULL;
static unsigned g_guest_messages = 0;
static unsigned g_input_messages = 0;
/* Diagnostic only: identify whether a game AV happens while a Win32 event
 * is synchronously forwarded from preview_present/msiEndFrame. Do not
 * delay, suppress or rearrange user input or game callbacks. */
static unsigned g_guest_callback_depth = 0;
static unsigned g_endframe_depth = 0;
static unsigned g_last_forwarded_msg = 0;
static unsigned g_last_forwarded_frame = 0;
static DWORD g_last_forwarded_tick = 0;
static unsigned g_focus_filtered_events = 0;
/* Nested game callbacks may render another frame before returning to their
 * outer WM_LBUTTONDOWN handler. Prevent recursively pumping more input during
 * that nested msiEndFrame; messages stay in the Win32 queue for the next
 * outermost frame. This prevents a proven triple WndProc reentry path. */
static unsigned g_inner_input_pumps_deferred = 0;
/* On the Windows 4:3 trial, two rapid Space presses preceded a buffered
 * WM_CHAR=Space inside a scene reload, then the OEM game dereferenced its
 * transiently NULL global [0050DCD0] at 0046D76D. Suppress ONLY this
 * late character during a short heap-allocation transition window; do
 * not drop KeyDown/KeyUp, other characters, or unrelated game states.
 * This narrow input-compatibility experiment remains unproven until
 * the same sequence is replayed on Windows with the rebuilt DLL. */
static DWORD g_last_heap_alloc_tick = 0;
static unsigned g_transition_space_char_ignored = 0;
static unsigned g_transition_space_char_evaluations = 0;
/* Passive instrumentation for the observed game AV at 0046D76D:
 * original OEM code loads EDX from [0050DCD0], then reads [EDX+0C].
 * Never assume the global holds a particular game-object type.
 * Sampling is enabled only with Debug Log=1 and a matching OEM code signature. */
static DWORD g_game_50dcd0_last = 0xFFFFFFFFu;
static DWORD g_game_50dcc0_last = 0xFFFFFFFFu;
static unsigned g_game_50dcd0_reports = 0;
/* v0.32: The OEM engine's input handler at 0043BCF9 writes via the object
 * pointer at 004BA4B8. A Wine startup click at frame 1 reaches that handler
 * before the object array is initialized (NULL), causing a write to 00000148.
 * Do not forward mouse events while game resources or this object array are
 * uninitialized. This affects only the hash-locked OEM probe; no EXE patch.
 * Keyboard input remains available to skip the intro. */
static unsigned g_early_mouse_ignored = 0;
static unsigned g_early_mouse_ready_logs = 0;
static int g_swallowed_left_down = 0;
static int g_swallowed_right_down = 0;

static int g_preview_created = 0;
static int g_viewport_reported=0;
static int g_vk_requested=0,g_vk_active=0,g_vk_attempted=0;
static unsigned g_vk_frames=0,g_vk_skipped=0,g_vk_ms=0,g_vk_recreates=0,g_vk_retry_streak=0;
/* Read-only candidate profiling: no GPU rasterization or texture copies.
 * One native frame in 60, only with Vulkan requested and logging enabled.
 * Logical bounds are conservatively mapped to the synthetic 1440x1080 target. */
static MsiGpuWave g_gpu_profile_wave;
/* One contiguous indexed8 opaque wave, manually or sampled in RAM.
 * Only explicit F12 writes files. Never used for presentation. */
#define GPU_CAPTURE_NATIVE_BYTES 512000u
#define GPU_CAPTURE_RAW_BYTES (2u*1024u*1024u)
#define GPU_CAPTURE_DECODED_COUNT (2u*1024u*1024u)
/* The one-draw recorder accepts at most 1 MiB of indexed8 texels plus
 * a 512-byte palette. It does not need the best-wave or CPU-mask buffers. */
#define GPU_CAPTURE_PILOT_TEXELS MSI_GPU_PILOT_DECODED_COUNT
#define GPU_CAPTURE_PILOT_RAW_BYTES MSI_GPU_PILOT_RAW_BYTES
_Static_assert(GPU_CAPTURE_NATIVE_BYTES==MSI_GPU_PILOT_NATIVE_BYTES,"native copy size");
static MsiGpuPilotStorage g_gpu_pilot_storage;
static unsigned g_gpu_pilot_allocations,g_gpu_pilot_reuses,g_gpu_pilot_releases;
typedef struct {
    MsiGpuDrawBatch batch;
    BYTE *before,*after,*written;
    /* Timed original CPU loops only for this owned reference wave. */
    DWORD loop_ticks;
    unsigned loop_draws,loop_untimed;
} MsiGpuCaptureSlot;
static MsiGpuCaptureSlot g_gpu_capture_slots[2];
static MsiGpuCaptureSlot *g_gpu_capture_current=&g_gpu_capture_slots[0];
static MsiGpuCaptureSlot *g_gpu_capture_best=&g_gpu_capture_slots[1];
#define g_gpu_capture_batch (g_gpu_capture_current->batch)
#define g_gpu_capture_before (g_gpu_capture_current->before)
#define g_gpu_capture_after (g_gpu_capture_current->after)
static unsigned g_gpu_capture_active,g_gpu_capture_draw,g_gpu_capture_best_changed,g_gpu_capture_waves;
static unsigned g_gpu_capture_sample;
static MsiGpuShadowSampler g_gpu_shadow_auto;
static struct {unsigned waves,copied,changed,preserved,rejected;} g_gpu_publish;
static unsigned g_gpu_capture_bypass;
static DWORD g_gpu_pilot_alloc_ticks,g_gpu_pilot_snapshot_ticks;
static struct {unsigned frames,last_frame,attempted,passed,failed,empty,written,changed,disabled;} g_gpu_bypass;
_Static_assert(sizeof(MsiGpuVertex)==32,"capture vertex ABI");
_Static_assert(sizeof(MsiGpuDraw)==24,"capture draw ABI");
static unsigned g_gpu_profile_enabled,g_gpu_profile_active;
static MsiGpuDeferAudit g_gpu_defer_audit,g_gpu_defer_strict;
static unsigned g_gpu_defer_strict_seen,g_gpu_defer_strict_state,g_gpu_defer_strict_early;
static unsigned g_gpu_defer_strict_numerical,g_gpu_defer_strict_alias;
static struct {
    unsigned sampled,eligible,opaque,alpha,keyed,flat,rgb888,other,affine;
    unsigned waves,opaque_waves,alpha_waves,singletons,max_wave,split;
    unsigned zplanes,remaps,sentinels,state_calls,heap_calls;
    unsigned tex4,tex8,tex16,drawn_frames,empty_frames;
    unsigned frame_candidates;
} g_gpu_profile;
static int g_presentation_smooth_enabled=0,g_presentation_smooth_failed=0,g_presentation_smooth_reported=0;
static int g_borderless_enabled=0;
static int g_borderless_active=0;
static int g_preview_class_registered = 0;
static int g_shutdown_started = 0;
static DWORD g_last_left_click_ms = 0;
static unsigned g_preview_presented = 0;
static unsigned g_preview_failures = 0;
static int g_last_tex_enable = -1;
/* MSI texture cache is indexed using CacheOffset. A one-KiB upload map
 * avoids treating never-uploaded texture pages as valid image data. */
static BYTE g_texture_uploaded[16*1024*1024/256];
static T_msiParameters g_state;
static int g_state_valid = 0;
static unsigned g_tex_uploads = 0, g_palette_uploads = 0;
static unsigned g_disabled_tex_uploads = 0, g_disabled_lut_uploads = 0;
static unsigned g_direct_cache_reads = 0;
static unsigned g_drawn_triangles = 0, g_drawn_pixels = 0;
static unsigned g_missing_textures = 0, g_rejected_triangles = 0;
static unsigned g_drawn_zplanes = 0, g_drawn_zpixels = 0, g_rejected_zplanes = 0;
static unsigned g_first_zplane_frame = 0xFFFFFFFFu;

static unsigned g_last_texture_detail_frame = 0;
/* Per-session diagnostics: CPU raster time, Win32 presentation, state/cache updates. */
static DWORD g_boot_tick = 0;
static DWORD g_raster_ms = 0, g_preview_ms = 0, g_state_ms = 0;
static DWORD g_last_triangle_tick = 0;
static unsigned g_last_triangle_frame = 0;
static unsigned g_no_geometry_logged = 0;


/* 32-bit Windows ABI: the first fields suffice for exception reporting. */
typedef struct _EXCEPTION_RECORD_MIN {
    DWORD ExceptionCode, ExceptionFlags;
    void* ExceptionRecord;
    void* ExceptionAddress;
    DWORD NumberParameters;
    DWORD ExceptionInformation[15];
} EXCEPTION_RECORD_MIN;
typedef struct _EXCEPTION_POINTERS_MIN {
    EXCEPTION_RECORD_MIN* ExceptionRecord;
    void* ContextRecord;
} EXCEPTION_POINTERS_MIN;
typedef struct _MEMORY_BASIC_INFORMATION32 {
    void* BaseAddress;
    void* AllocationBase;
    DWORD AllocationProtect;
    DWORD RegionSize;
    DWORD State, Protect, Type;
} MEMORY_BASIC_INFORMATION32;
typedef struct {
  DWORD ContextFlags, Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
  struct {DWORD ControlWord,StatusWord,TagWord,ErrorOffset,ErrorSelector,DataOffset,DataSelector;
      BYTE RegisterArea[80]; DWORD Cr0NpxState;} FloatSave;
  DWORD SegGs,SegFs,SegEs,SegDs,Edi,Esi,Ebx,Edx,Ecx,Eax,Ebp,Eip,SegCs,EFlags,Esp,SegSs;
  BYTE ExtendedRegisters[512];
} CONTEXT32_MIN;
static LONG __stdcall probe_exception(EXCEPTION_POINTERS_MIN* e);
static BOOL readable_span(const void *p, DWORD len);


static DWORD slen(const char*s){DWORD n=0; if(!s)return 0;while(s[n] && n<1024)n++;return n;}
static void putstr(char *dest,DWORD *p,const char *s){DWORD i=0;while(s && s[i] && *p<900){dest[(*p)++]=s[i++];}}
static void putunsigned(char*dest,DWORD*p,unsigned num){char tmp[16];int n=0;do{tmp[n++]=(char)('0'+num%10);num/=10;}while(num&&n<16);while(n&&*p<900)dest[(*p)++]=tmp[--n];}
static void puthex(char*dest,DWORD*p,unsigned num){const char*digits="0123456789ABCDEF";putstr(dest,p,"0x");for(int i=7;i>=0;i--)if(*p<900)dest[(*p)++]=digits[(num>>(i*4))&15];}
static void ensure_log(void){
    if(g_log_attempted)return;g_log_attempted=1;
    /* Log=0 in root ScorchedPlanetFix.ini is passed by the launcher.
     * Do not open a file, register a debug-only VEH, or write to stderr. */
    {char setting[8]={0};
     DWORD n=GetEnvironmentVariableA("MATROX_MSI_LOG",setting,sizeof(setting));
     if(n!=1u || setting[0]!='1')return;
    }
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(n==0||n>=1023)return;
    DWORD i=n; while(i>0 && path[i-1]!='\\' && path[i-1]!='/')i--;
    const char *name="..\\ScorchedPlanetFix.log";
    DWORD k=0;while(name[k] && i+k<1022){path[i+k]=name[k];k++;}
    path[i+k]=0;
    g_log=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(g_log==INVALID_HANDLE_VALUE)g_log=NULL;
    if(g_log){const char*msg="ScorchedPlanet Matrox MSI95 PROBE 0.35 | optional MSI95 texture modulation and input guard\r\n";
      DWORD written;WriteFile(g_log,msg,slen(msg),&written,NULL);
      g_veh_registered = AddVectoredExceptionHandler(1,(void*)probe_exception)!=NULL;
      const char*status=g_veh_registered?"VEH registration OK\r\n":"VEH registration FAILED\r\n";
      WriteFile(g_log,status,slen(status),&written,NULL);
    }
}
static void logtext(const char*msg){ensure_log();if(!g_log)return;DWORD n=slen(msg),w;WriteFile(g_log,msg,n,&w,NULL);WriteFile(g_log,"\r\n",2,&w,NULL);}

/* Self-contained, read-only host identification for stand-alone MSI95 logs.
 * The launcher's SYS-* inventory lives in a separate installer log, so
 * runtime crash reports otherwise cannot prove whether Wine/Proton was used.
 * Never record Wine prefixes, Steam paths, usernames, or other env contents.
 * No additional work is performed when [Debug] Log=0. */
static void log_runtime_host(void){
    static int reported=0;
    if(reported || !g_log)return;
    reported=1;
    HANDLE ntdll=GetModuleHandleA("ntdll.dll");
    void* wine_version=ntdll?GetProcAddress(ntdll,"wine_get_version"):NULL;
    if(!wine_version){
        logtext("SYS-OS: runtime=Windows native");
        logtext("SYS-WINE: not detected");
        logtext("SYS-HOST: OS=Windows");
        logtext("SYS-PROTON: not detected (native Windows)");
        return;
    }
    logtext("SYS-OS: runtime=Wine (Win32 compatibility layer)");
    {typedef const char* (__cdecl *wine_version_fn)(void);
     const char*raw=((wine_version_fn)wine_version)();
     char buf[160];DWORD p=0;
     putstr(buf,&p,"SYS-WINE: version=");
     if(raw){unsigned i=0;while(raw[i]&&i<80u&&p<150u){
         unsigned char ch=(unsigned char)raw[i++];
         if((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||
            ch=='.'||ch=='-'||ch=='_'||ch=='+'||ch==' '||ch=='('||ch==')')buf[p++]=(char)ch;
         else buf[p++]='?';
       }}
     if(p==18u)putstr(buf,&p,"Unavailable");
     buf[p]=0;logtext(buf);
    }
    {typedef void (__cdecl *wine_host_fn)(const char**,const char**);
     void* host_fn=GetProcAddress(ntdll,"wine_get_host_version");
     if(host_fn){
         const char* host_os=NULL;const char* host_release=NULL;
         ((wine_host_fn)host_fn)(&host_os,&host_release);
         char buf[140];DWORD p=0;putstr(buf,&p,"SYS-HOST: OS=");
         if(host_os){unsigned i=0;while(host_os[i]&&i<50u&&p<125u){
             unsigned char ch=(unsigned char)host_os[i++];
             buf[p++]=((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||
                       (ch>='0'&&ch<='9')||ch=='-')?(char)ch:'?';
           }}
         if(p==13u)putstr(buf,&p,"Unavailable");
         buf[p]=0;logtext(buf);
     }else logtext("SYS-HOST: OS=Unavailable (Wine host API not exported)");
    }
    {char tmp[2]={0};
     const char* steam_vars[]={"STEAM_COMPAT_DATA_PATH","STEAM_COMPAT_CLIENT_INSTALL_PATH",
                               "STEAM_COMPAT_TOOL_PATHS","PROTON_VERSION"};
     int steam_hint=0;
     for(unsigned i=0;i<4u;i++)if(GetEnvironmentVariableA(steam_vars[i],tmp,sizeof(tmp))>0u){
         steam_hint=1;break;
     }
     logtext(steam_hint?
        "SYS-PROTON: Steam compatibility environment=yes (environment hint, unverified)":
        "SYS-PROTON: environment hint=not detected (cannot exclude Proton)");
    }
}

/* Observe first-chance access violations only; never alter exception handling.
 * A first-chance AV can be handled and is not automatically the fatal crash.
 * Cap output to prevent excessive noise or recursive exceptions during logging. */
static LONG __stdcall probe_exception(EXCEPTION_POINTERS_MIN* e){
    if(!g_log || !e || !e->ExceptionRecord ||
       e->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION ||
       g_veh_count>=8) return EXCEPTION_CONTINUE_SEARCH;
    g_veh_count++;
    EXCEPTION_RECORD_MIN *r=e->ExceptionRecord;
    char buf[940];DWORD n=0;
    putstr(buf,&n,"FIRST_CHANCE_AV #");putunsigned(buf,&n,g_veh_count);
    putstr(buf,&n," code=");puthex(buf,&n,r->ExceptionCode);
    putstr(buf,&n," ip=");puthex(buf,&n,(DWORD)r->ExceptionAddress);
    if(r->NumberParameters>=2){
        putstr(buf,&n," operation=");putunsigned(buf,&n,r->ExceptionInformation[0]);
        putstr(buf,&n," address=");puthex(buf,&n,r->ExceptionInformation[1]);
    }
    MEMORY_BASIC_INFORMATION32 mem;
    if(VirtualQuery(r->ExceptionAddress,&mem,sizeof(mem))==sizeof(mem) && mem.AllocationBase){
        putstr(buf,&n," module_base=");puthex(buf,&n,(DWORD)mem.AllocationBase);
        putstr(buf,&n," module_offset=");puthex(buf,&n,(DWORD)r->ExceptionAddress-(DWORD)mem.AllocationBase);
        char module[512];DWORD count=GetModuleFileNameA(mem.AllocationBase,module,511);
        if(count>0 && count<511){module[count]=0;putstr(buf,&n," module=");putstr(buf,&n,module);}
    }
    buf[n]=0;
    DWORD wr=0;WriteFile(g_log,buf,n,&wr,NULL);
    WriteFile(g_log,"\r\n",2,&wr,NULL);
    { char state[940];DWORD k=0;
      putstr(state,&k,"MSI_BUFFER_STATE color=");puthex(state,&k,(DWORD)g_info.msiColor.pMem);
      putstr(state,&k," backing_color=");puthex(state,&k,(DWORD)g_color);
      putstr(state,&k," guest_color=");puthex(state,&k,(DWORD)g_color_guest);
      putstr(state,&k," depth=");puthex(state,&k,(DWORD)g_info.msiDepth.pMem);
      putstr(state,&k," backing_depth=");puthex(state,&k,(DWORD)g_depth);
      putstr(state,&k," frames=");putunsigned(state,&k,g_frames);
      state[k]=0;WriteFile(g_log,state,k,&wr,NULL);WriteFile(g_log,"\r\n",2,&wr,NULL);
    }
    if(e->ContextRecord && readable_span(e->ContextRecord,sizeof(CONTEXT32_MIN))){
      CONTEXT32_MIN *ctx=(CONTEXT32_MIN*)e->ContextRecord;
      char regs[560];DWORD k=0;
      putstr(regs,&k,"AV_REGISTERS eax=");puthex(regs,&k,ctx->Eax);
      putstr(regs,&k," ebx=");puthex(regs,&k,ctx->Ebx);
      putstr(regs,&k," ecx=");puthex(regs,&k,ctx->Ecx);
      putstr(regs,&k," edx=");puthex(regs,&k,ctx->Edx);
      putstr(regs,&k," esi=");puthex(regs,&k,ctx->Esi);
      putstr(regs,&k," edi=");puthex(regs,&k,ctx->Edi);
      putstr(regs,&k," ebp=");puthex(regs,&k,ctx->Ebp);
      putstr(regs,&k," esp=");puthex(regs,&k,ctx->Esp);
      putstr(regs,&k," elapsed_ms=");putunsigned(regs,&k,GetTickCount()-g_boot_tick);
      regs[k]=0;WriteFile(g_log,regs,k,&wr,NULL);WriteFile(g_log,"\r\n",2,&wr,NULL);
      /* Read-only snapshot of the original game's scene-object manager.
       * In both observed faults it announces live elements in [+0x1C]
       * while its element-table pointer at [+0x0C] is NULL. The parent
       * is EBP at 0x00482DEC, EBX at 0x0048302D; never modify context. */
      if(ctx->Eip==0x00482DECu || ctx->Eip==0x0048302Du){
        DWORD parent=ctx->Eip==0x00482DECu ? ctx->Ebp : ctx->Ebx;
        char info[360];k=0;
        putstr(info,&k,"AV_SCENE_LIST manager=");puthex(info,&k,parent);
        putstr(info,&k," index=");putunsigned(info,&k,ctx->Edx);
        if(parent && readable_span((void*)parent,0x24u)){
          const DWORD *p=(const DWORD*)parent;
          putstr(info,&k," data=");puthex(info,&k,p[3]);
          putstr(info,&k," head=");puthex(info,&k,p[6]);
          putstr(info,&k," count=");putunsigned(info,&k,p[7]);
          putstr(info,&k," capacity=");putunsigned(info,&k,p[8]);
          putstr(info,&k," flags=");puthex(info,&k,p[0]);
        }else putstr(info,&k," inaccessible");
        info[k]=0;WriteFile(g_log,info,k,&wr,NULL);
        WriteFile(g_log,"\r\n",2,&wr,NULL);
      }
      { char events[320];k=0;
        putstr(events,&k,"AV_INPUT_CONTEXT endframe_depth=");
        putunsigned(events,&k,g_endframe_depth);
        putstr(events,&k," guest_callback_depth=");
        putunsigned(events,&k,g_guest_callback_depth);
        putstr(events,&k," last_msg=");
        puthex(events,&k,g_last_forwarded_msg);
        putstr(events,&k," last_frame=");
        putunsigned(events,&k,g_last_forwarded_frame);
        putstr(events,&k," last_msg_age_ms=");
        putunsigned(events,&k,g_last_forwarded_tick ?
                    GetTickCount()-g_last_forwarded_tick : 0xFFFFFFFFu);
        events[k]=0;WriteFile(g_log,events,k,&wr,NULL);
        WriteFile(g_log,"\r\n",2,&wr,NULL);
      }
      if(ctx->Eip==0x0046D76Du){
        char game[410];k=0;
        putstr(game,&k,"AV_GAME_GLOBAL_50DCD0 ");
        if(readable_span((const void*)0x0050DCC0u,0x18u)){
          const volatile DWORD *p=(const volatile DWORD*)0x0050DCC0u;
          putstr(game,&k,"counter_50DCC0=");puthex(game,&k,p[0]);
          putstr(game,&k," raw_50DCC4=");puthex(game,&k,p[1]);
          putstr(game,&k," raw_50DCC8=");puthex(game,&k,p[2]);
          putstr(game,&k," ptr_50DCD0=");puthex(game,&k,p[4]);
          putstr(game,&k," raw_50DCD4=");puthex(game,&k,p[5]);
          putstr(game,&k," last_sampled_ptr=");puthex(game,&k,g_game_50dcd0_last);
          putstr(game,&k," last_sampled_counter=");puthex(game,&k,g_game_50dcc0_last);
        }else putstr(game,&k,"unreadable");
        game[k]=0;WriteFile(g_log,game,k,&wr,NULL);
        WriteFile(g_log,"\r\n",2,&wr,NULL);
        if(readable_span((const void*)ctx->Esp,48u)){
          char stack2[360];k=0;
          putstr(stack2,&k,"AV_GAME_STACK first12=");
          const DWORD *sp=(const DWORD*)ctx->Esp;
          for(unsigned i=0;i<12u;i++){putstr(stack2,&k," ");puthex(stack2,&k,sp[i]);}
          stack2[k]=0;WriteFile(g_log,stack2,k,&wr,NULL);
          WriteFile(g_log,"\r\n",2,&wr,NULL);
        }
      }
      { char heap[240];k=0;
        putstr(heap,&k,"AV_HEAP_TRACKER live=");putunsigned(heap,&k,g_heap_count);
        putstr(heap,&k," capacity=");putunsigned(heap,&k,g_heap_capacity);
        putstr(heap,&k," peak_live=");putunsigned(heap,&k,g_heap_peak);
        heap[k]=0;WriteFile(g_log,heap,k,&wr,NULL);
        WriteFile(g_log,"\r\n",2,&wr,NULL);
      }
      if(readable_span((void*)ctx->Esp,24u)){
        char stack[300];k=0;putstr(stack,&k,"AV_STACK first6=");
        DWORD* sp=(DWORD*)ctx->Esp;
        for(unsigned i=0;i<6;i++){putstr(stack,&k," ");puthex(stack,&k,sp[i]);}
        stack[k]=0;WriteFile(g_log,stack,k,&wr,NULL);WriteFile(g_log,"\r\n",2,&wr,NULL);
      }
      /* Read only the faulting instruction window on a first-chance AV.
       * Never change executable bytes or exception-handling semantics.
       * Keep this trace guarded by the normal Debug Log=1 condition. */
      /* Capture the caller's stack and code around its return address.
       * This is read-only and does not patch/skip the crash. In the known
       * 0x00494965 case, [esp+8] is the invalid first function argument. */
      if(ctx->Eip==0x00494965u && readable_span((void*)ctx->Esp,12u)){
        const DWORD *sp=(const DWORD*)ctx->Esp;
        DWORD ret=sp[1];
        char caller[200];k=0;
        putstr(caller,&k,"AV_CALLER saved_esi=");puthex(caller,&k,sp[0]);
        putstr(caller,&k," return=");puthex(caller,&k,ret);
        putstr(caller,&k," first_arg=");puthex(caller,&k,sp[2]);
        caller[k]=0;
        WriteFile(g_log,caller,k,&wr,NULL);
        WriteFile(g_log,"\r\n",2,&wr,NULL);
        if(ret>=24u && readable_span((void*)(ret-24u),48u)){
          static const char hex_digits[]="0123456789ABCDEF";
          const BYTE* code=(const BYTE*)(ret-24u);
          char caller_bytes[220];k=0;
          putstr(caller_bytes,&k,"AV_CALLER_CODE before24_and_after24=");
          for(unsigned i=0;i<48u;i++){
            if(i==24u)putstr(caller_bytes,&k," |");
            caller_bytes[k++]=' ';
            caller_bytes[k++]=hex_digits[(code[i]>>4)&15u];
            caller_bytes[k++]=hex_digits[code[i]&15u];
          }
          caller_bytes[k]=0;
          WriteFile(g_log,caller_bytes,k,&wr,NULL);
          WriteFile(g_log,"\r\n",2,&wr,NULL);
        }
      }
      if(ctx->Eip>=16u && readable_span((void*)(ctx->Eip-16u),32u)){
        static const char hex_digits[]="0123456789ABCDEF";
        const BYTE* code=(const BYTE*)(ctx->Eip-16u);
        char instruction[180];k=0;
        putstr(instruction,&k,"AV_CODE before16_and_after16=");
        for(unsigned i=0;i<32u;i++){
            if(i==16u)putstr(instruction,&k," |");
            instruction[k++]=' ';
            instruction[k++]=hex_digits[(code[i]>>4)&15u];
            instruction[k++]=hex_digits[code[i]&15u];
        }
        instruction[k]=0;
        WriteFile(g_log,instruction,k,&wr,NULL);
        WriteFile(g_log,"\r\n",2,&wr,NULL);
      }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void lognum(const char*name,unsigned a,const char *label2,unsigned b){
  char buf[940];DWORD p=0;putstr(buf,&p,name);putstr(buf,&p," ");putunsigned(buf,&p,a);
  if(label2){putstr(buf,&p," ");putstr(buf,&p,label2);putstr(buf,&p," ");putunsigned(buf,&p,b);}buf[p]=0;logtext(buf);
}
/* No rendering side effects: collect hypothetical waves, never submit. */
static void gpu_profile_flush(void){
    msi_gpu_defer_barrier(&g_gpu_defer_audit);
    msi_gpu_defer_barrier(&g_gpu_defer_strict);
    unsigned count=g_gpu_profile_wave.count;
    if(!count)return;
    g_gpu_profile.waves++;
    if(g_gpu_profile_wave.needs_prior)g_gpu_profile.alpha_waves++;
    else g_gpu_profile.opaque_waves++;
    if(count==1u)g_gpu_profile.singletons++;
    if(count>g_gpu_profile.max_wave)g_gpu_profile.max_wave=count;
    msi_gpu_wave_reset(&g_gpu_profile_wave);
}
static void gpu_dual_thread_observe(void){
    if(g_gpu_profile_active&&g_color_guest&&g_color_guest!=g_color){
        g_gpu_dual_thread_checks++;
        g_gpu_dual_foreign+=GetCurrentThreadId()!=g_gpu_dual_owner;
    }
}
static void gpu_profile_finish(void){
    if(!g_gpu_profile_active)return;
    if(g_color_guest&&g_color_guest!=g_color){
        g_gpu_dual_checks++;
        if(!msi_gpu_snapshot_equal((const BYTE*)g_color_guest,(const BYTE*)g_color,
                                    GPU_CAPTURE_NATIVE_BYTES)){
            g_gpu_dual_mismatch++;
            logtext("GPU_DUAL_VIEW MISMATCH sampled alias comparison");
        }
    }
    gpu_profile_flush();
    if(g_gpu_profile.frame_candidates)g_gpu_profile.drawn_frames++;
    else g_gpu_profile.empty_frames++;
    g_gpu_profile_active=0;
}
static void gpu_profile_candidate(const T_msiVertex *a,const T_msiVertex *b,const T_msiVertex *c,
    int left,int top,int right,int bottom,
    int width,int height,int textured,int modulate,int perspective,int bilinear,unsigned planes,unsigned opacity){
    if(!g_gpu_profile_active||g_zplane_draw_context)return;
    unsigned pointer_state=msi_gpu_defer_pointer((DWORD)g_info.msiColor.pMem,
        (DWORD)g_info.msiDepth.pMem,(DWORD)g_color_guest,(DWORD)g_depth);
    int defer_eligible=width==640&&height==400&&textured&&planes==8u&&opacity==100u&&
        perspective&&bilinear&&!(modulate&&g_rgb888_lighting_enabled)&&
        !g_state.msiDepth.Enable&&!g_state.msiDepth.Protect&&!g_state.msiColor.Protect&&
        !g_state.msiTexture.Decal&&!g_endframe_depth&&!g_guest_callback_depth;
    msi_gpu_defer_candidate(&g_gpu_defer_audit,pointer_state,
        (unsigned)g_state.msiTexture.Transparency,defer_eligible);
    /* Strict opportunity: no intervening parameter API, invalid triangle,
     * aliased vertex record or numerically inadmissible geometry. */
    g_gpu_defer_strict_seen=1;
    int strict=defer_eligible;
    if(strict){
        const T_msiVertex *in[3]={a,b,c};MsiGpuVertex v[3];
        unsigned flags=9u|(g_state.msiTexture.Clamp_u?2u:0u)|
            (g_state.msiTexture.Clamp_v?4u:0u)|(modulate?16u:0u)|
            (modulate&&g_msi_dither_enabled&&g_state.msiColor.Dither?32u:0u)|
            (g_state.msiTexture.Transparency?64u:0u);
        unsigned alias=0;
        for(unsigned i=0;i<3u;i++){
            alias|=msi_gpu_defer_overlap((uintptr_t)in[i],sizeof(*in[i]),
                (uintptr_t)g_color_guest,GPU_CAPTURE_NATIVE_BYTES);
            v[i].xy[0]=in[i]->x;v[i].xy[1]=in[i]->y;
            v[i].uv[0]=in[i]->u;v[i].uv[1]=in[i]->v;
            v[i].iw=in[i]->invW;v[i].m[0]=in[i]->mr;v[i].m[1]=in[i]->mg;v[i].m[2]=in[i]->mb;
        }
        if(alias){g_gpu_defer_strict_alias++;strict=0;}
        else if(!msi_gpu_defer_draw_admissible(v,(MsiGpuRect){(unsigned)left,(unsigned)top,
                 (unsigned)right,(unsigned)bottom},flags)){
            g_gpu_defer_strict_numerical++;strict=0;
        }
    }
    msi_gpu_defer_candidate(&g_gpu_defer_strict,pointer_state,
        (unsigned)g_state.msiTexture.Transparency,strict);
    if(width!=640||height!=400||opacity>100u){g_gpu_profile.other++;gpu_profile_flush();return;}
    if(!textured){g_gpu_profile.flat++;gpu_profile_flush();return;}
    if(modulate&&g_rgb888_lighting_enabled){g_gpu_profile.rgb888++;gpu_profile_flush();return;}
    if(!perspective&&!bilinear){g_gpu_profile.affine++;gpu_profile_flush();return;}
    if(planes!=8u||opacity!=100u){g_gpu_profile.other++;gpu_profile_flush();return;}
    MsiGpuRect rect={(unsigned)left*1440u/640u,(unsigned)top*1080u/400u,
        ((unsigned)right*1440u+639u)/640u,((unsigned)bottom*1080u+399u)/400u};
    unsigned prior=opacity<100u;
    int result=msi_gpu_wave_push(&g_gpu_profile_wave,rect,prior);
    if(result==MSI_GPU_WAVE_FLUSH){
        g_gpu_profile.split++;gpu_profile_flush();
        result=msi_gpu_wave_push(&g_gpu_profile_wave,rect,prior);
    }
    if(result!=MSI_GPU_WAVE_ACCEPT){g_gpu_profile.other++;gpu_profile_flush();return;}
    g_gpu_profile.eligible++;g_gpu_profile.frame_candidates++;
    if(prior)g_gpu_profile.alpha++;else g_gpu_profile.opaque++;
    if(g_state.msiTexture.Transparency)g_gpu_profile.keyed++;
    if(planes==4u)g_gpu_profile.tex4++;else if(planes==8u)g_gpu_profile.tex8++;else g_gpu_profile.tex16++;
}
static void gpu_profile_report(void){
    if(!g_gpu_profile_enabled)return;
    gpu_profile_finish();
    lognum("GPU_PROFILE sampled_frames=",g_gpu_profile.sampled,"frames_with_candidates=",g_gpu_profile.drawn_frames);
    lognum("GPU_PROFILE eligible=",g_gpu_profile.eligible,"empty_frames=",g_gpu_profile.empty_frames);
    lognum("GPU_PROFILE opaque=",g_gpu_profile.opaque,"alpha=",g_gpu_profile.alpha);
    lognum("GPU_PROFILE keyed=",g_gpu_profile.keyed,"flat_excluded=",g_gpu_profile.flat);
    lognum("GPU_PROFILE rgb888_excluded=",g_gpu_profile.rgb888,"other_excluded=",g_gpu_profile.other);
    lognum("GPU_PROFILE tex4=",g_gpu_profile.tex4,"tex8=",g_gpu_profile.tex8);
    lognum("GPU_PROFILE tex16=",g_gpu_profile.tex16,"affine_excluded=",g_gpu_profile.affine);
    lognum("GPU_PROFILE waves=",g_gpu_profile.waves,"singletons=",g_gpu_profile.singletons);
    lognum("GPU_PROFILE opaque_waves=",g_gpu_profile.opaque_waves,"alpha_waves=",g_gpu_profile.alpha_waves);
    lognum("GPU_PROFILE max_wave=",g_gpu_profile.max_wave,"planner_splits=",g_gpu_profile.split);
    lognum("GPU_PROFILE zplane_barriers=",g_gpu_profile.zplanes,"null_remaps=",g_gpu_profile.remaps);
    lognum("GPU_PROFILE sentinel_barriers=",g_gpu_profile.sentinels,"heap_barriers=",g_gpu_profile.heap_calls);
    lognum("GPU_PROFILE state_calls=",g_gpu_profile.state_calls,NULL,0);
    lognum("GPU_DEFER_AUDIT eligible=",g_gpu_defer_audit.eligible,"mapped=",g_gpu_defer_audit.mapped);
    lognum("GPU_DEFER_AUDIT invalidated=",g_gpu_defer_audit.invalidated,"unknown=",g_gpu_defer_audit.unknown);
    lognum("GPU_DEFER_AUDIT excluded=",g_gpu_defer_audit.excluded,"groups=",g_gpu_defer_audit.groups);
    lognum("GPU_DEFER_AUDIT group_draws=",g_gpu_defer_audit.group_draws,"multi_groups=",g_gpu_defer_audit.multi_groups);
    lognum("GPU_DEFER_AUDIT multi_draws=",g_gpu_defer_audit.multi_draws,"max_group=",g_gpu_defer_audit.max_group);
    lognum("GPU_DEFER_AUDIT split_key=",g_gpu_defer_audit.split_key,"split_limit=",g_gpu_defer_audit.split_limit);
    lognum("GPU_DEFER_AUDIT barriers=",g_gpu_defer_audit.barriers,"remaps=",g_gpu_defer_audit.remaps);
    lognum("GPU_DEFER_AUDIT remaps_invalidated=",g_gpu_defer_audit.remaps_invalidated,"counts_ok=",msi_gpu_defer_counts_ok(&g_gpu_defer_audit));
    lognum("GPU_DEFER_STRICT eligible=",g_gpu_defer_strict.eligible,"mapped=",g_gpu_defer_strict.mapped);
    lognum("GPU_DEFER_STRICT invalidated=",g_gpu_defer_strict.invalidated,"unknown=",g_gpu_defer_strict.unknown);
    lognum("GPU_DEFER_STRICT groups=",g_gpu_defer_strict.groups,"group_draws=",g_gpu_defer_strict.group_draws);
    lognum("GPU_DEFER_STRICT multi_groups=",g_gpu_defer_strict.multi_groups,"multi_draws=",g_gpu_defer_strict.multi_draws);
    lognum("GPU_DEFER_STRICT max_group=",g_gpu_defer_strict.max_group,"counts_ok=",msi_gpu_defer_counts_ok(&g_gpu_defer_strict));
    lognum("GPU_DEFER_STRICT state_barriers=",g_gpu_defer_strict_state,"early_barriers=",g_gpu_defer_strict_early);
    lognum("GPU_DEFER_STRICT numerical_reject=",g_gpu_defer_strict_numerical,"vertex_alias_reject=",g_gpu_defer_strict_alias);
    lognum("GPU_DUAL_VIEW checks=",g_gpu_dual_checks,"mismatch=",g_gpu_dual_mismatch);
    lognum("GPU_DUAL_VIEW thread_checks=",g_gpu_dual_thread_checks,"foreign=",g_gpu_dual_foreign);
}
/* Sparse histogram: absent values have zero requests. Emit at F12 and exit
 * only, avoiding additional per-triangle logging or duplicated Z-plane halves. */
static void log_opacity_histogram(const char *scope){
    char buf[160];DWORD n=0;
    putstr(buf,&n,"MSI_OPACITY_BEGIN scope=");putstr(buf,&n,scope);
    putstr(buf,&n," frame=");putunsigned(buf,&n,g_frames);
    buf[n]=0;logtext(buf);
    for(unsigned i=0;i<256u;i++){
        if(!g_triangle_opacity[i] && !g_zplane_opacity[i])continue;
        n=0;putstr(buf,&n,"MSI_OPACITY value=");putunsigned(buf,&n,i);
        putstr(buf,&n," triangles=");putunsigned(buf,&n,g_triangle_opacity[i]);
        putstr(buf,&n," zplanes=");putunsigned(buf,&n,g_zplane_opacity[i]);
        if(g_triangle_opacity[i]==0xFFFFFFFFu || g_zplane_opacity[i]==0xFFFFFFFFu)
            putstr(buf,&n," saturated=1");
        buf[n]=0;logtext(buf);
    }
    logtext("MSI_OPACITY_END");
}
/* Strictly diagnostic, signature-locked to the OEM Matrox instruction
 * sequence seen in the Wine AV. Do not use this probe to repair or skip any
 * game dereference, and never read arbitrary player data. */
static BOOL game_global_probe_supported(void){
    const BYTE *load=(const BYTE*)0x0046D761u;
    const BYTE *cmp=(const BYTE*)0x0046D76Du;
    return readable_span(load,6u) && readable_span(cmp,3u) &&
        load[0]==0x8Bu && load[1]==0x15u &&
        load[2]==0xD0u && load[3]==0xDCu &&
        load[4]==0x50u && load[5]==0x00u &&
        cmp[0]==0x39u && cmp[1]==0x42u && cmp[2]==0x0Cu;
}
static void sample_game_global_50dcd0(const char *phase){
    if(!g_log || g_game_50dcd0_reports>=96u ||
       !game_global_probe_supported() ||
       !readable_span((const void*)0x0050DCC0u,0x14u))return;
    const volatile DWORD *globals=(const volatile DWORD*)0x0050DCC0u;
    DWORD counter=globals[0],ptr=globals[4];
    BOOL changed=ptr!=g_game_50dcd0_last;
    if(!changed && (g_frames%120u!=0u || counter==g_game_50dcc0_last))return;
    char info[320];DWORD k=0;
    putstr(info,&k,"GAME_GLOBAL_50DCD0 phase=");
    putstr(info,&k,phase);
    putstr(info,&k," frame=");putunsigned(info,&k,g_frames);
    putstr(info,&k," ptr=");puthex(info,&k,ptr);
    putstr(info,&k," prior_ptr=");puthex(info,&k,g_game_50dcd0_last);
    putstr(info,&k," counter_50DCC0=");puthex(info,&k,counter);
    putstr(info,&k," callback_depth=");putunsigned(info,&k,g_guest_callback_depth);
    info[k]=0;logtext(info);
    g_game_50dcd0_last=ptr;
    g_game_50dcc0_last=counter;
    g_game_50dcd0_reports++;
}
static void loginit(LONG w,LONG h,LONG planes,BOOL z,BOOL dbg){
  char buf[940];DWORD p=0;putstr(buf,&p,"msiInit width=");putunsigned(buf,&p,(unsigned)w);
  putstr(buf,&p," height=");putunsigned(buf,&p,(unsigned)h);
  putstr(buf,&p," planes=");putunsigned(buf,&p,(unsigned)planes);
  putstr(buf,&p," zbuffer=");putunsigned(buf,&p,(unsigned)z);
  putstr(buf,&p," debug=");putunsigned(buf,&p,(unsigned)dbg);
  buf[p]=0;logtext(buf);
}
static void zero(void*ptr,unsigned n){BYTE*p=(BYTE*)ptr;for(unsigned i=0;i<n;i++)p[i]=0;}
static void* alloc(unsigned n){return n?VirtualAlloc(NULL,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE):NULL;}
/* Map an anonymous section twice. Only diagnostic native RGB565 sessions
 * use it; failure before exposing a pointer safely selects legacy allocation.
 * No VirtualProtect or exception interception here. */
static void color_allocate(unsigned bytes,int dual){
    g_color=g_color_guest=NULL;
    if(dual){
        HANDLE mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,bytes,NULL);
        if(mapping){
            g_color=MapViewOfFile(mapping,6u,0,0,bytes);
            g_color_guest=MapViewOfFile(mapping,6u,0,0,bytes);
            CloseHandle(mapping);
        }
        int ok=g_color&&g_color_guest&&g_color!=g_color_guest;
        if(ok){
            volatile WORD *a=(volatile WORD*)g_color,*b=(volatile WORD*)g_color_guest;
            unsigned last=bytes/2u-1u;
            a[0]=0x1234u;b[last]=0x5678u;
            ok=b[0]==0x1234u&&a[last]==0x5678u;
            a[0]=0;b[last]=0;
        }
        if(ok){logtext("GPU_DUAL_VIEW mapped=1 guard=0");return;}
        if(g_color_guest&&g_color_guest!=g_color)UnmapViewOfFile(g_color_guest);
        if(g_color)UnmapViewOfFile(g_color);
        g_color=g_color_guest=NULL;
        logtext("GPU_DUAL_VIEW mapped=0 guard=0 fallback=1");
    }
    g_color=alloc(bytes);g_color_guest=g_color;
}

/* A reserved sentinel distinguishes an absent clock from a real zero
 * interval. Diagnostics only, called on the existing sampled paths. */
static MSI_INT64 gpu_cost_now(void){
    MSI_LARGE_INTEGER q;
    return g_cap_frequency&&QueryPerformanceCounter(&q)&&q.QuadPart>0?q.QuadPart:0;
}
static DWORD gpu_cost_elapsed(MSI_INT64 begin){
    MSI_INT64 end=begin?gpu_cost_now():0;
    return begin&&end>=begin&&end-begin<4294967295LL?(DWORD)(end-begin):UINT32_MAX;
}
static void gpu_capture_clear_loop(MsiGpuCaptureSlot *slot){
    slot->loop_ticks=slot->loop_draws=slot->loop_untimed=0;
}
static void gpu_pilot_free(void *p){if(p)VirtualFree(p,0,MEM_RELEASE);}
static void gpu_pilot_storage_destroy(void){
    if(g_gpu_pilot_storage.before){
        msi_gpu_pilot_storage_destroy(&g_gpu_pilot_storage,gpu_pilot_free);
        g_gpu_pilot_releases++;
    }
}
static void gpu_capture_release(void){
    msi_gpu_shadow_sampler_finish(&g_gpu_shadow_auto,MSI_SHADOW_SKIP);
    for(unsigned i=0;i<2u;i++){
        MsiGpuCaptureSlot *slot=&g_gpu_capture_slots[i];
        /* The sparse pilot borrows its session-owned storage; auto/F12
         * slots retain their independent original allocation lifetime. */
        if(slot->before&&slot->before!=g_gpu_pilot_storage.before)VirtualFree(slot->before,0,MEM_RELEASE);
        if(slot->after)VirtualFree(slot->after,0,MEM_RELEASE);
        if(slot->written)VirtualFree(slot->written,0,MEM_RELEASE);
        if(slot->batch.snapshots.raw&&slot->batch.snapshots.raw!=g_gpu_pilot_storage.raw)VirtualFree(slot->batch.snapshots.raw,0,MEM_RELEASE);
        if(slot->batch.snapshots.decoded&&slot->batch.snapshots.decoded!=g_gpu_pilot_storage.decoded)VirtualFree(slot->batch.snapshots.decoded,0,MEM_RELEASE);
        slot->before=slot->after=slot->written=NULL;
        slot->batch.snapshots.raw=NULL;slot->batch.snapshots.decoded=NULL;
        msi_gpu_draw_batch_reset(&slot->batch);
        gpu_capture_clear_loop(slot);
    }
    g_gpu_pilot_alloc_ticks=g_gpu_pilot_snapshot_ticks=UINT32_MAX;
    g_gpu_capture_current=&g_gpu_capture_slots[0];g_gpu_capture_best=&g_gpu_capture_slots[1];
    g_gpu_capture_active=g_gpu_capture_draw=g_gpu_capture_best_changed=g_gpu_capture_waves=g_gpu_capture_sample=g_gpu_capture_bypass=0;
    if(g_gpu_bypass.disabled||g_gpu_shadow_auto.disabled||g_gpu_bypass.attempted>=12u||g_gpu_bypass.frames>=36u)
        gpu_pilot_storage_destroy();
}
static void gpu_capture_begin(void){
    gpu_capture_release();
    if(!g_vk_requested||!g_log||!g_color||
       g_info.msiColor.Pitch!=1280||g_info.msiColor.MemSize!=GPU_CAPTURE_NATIVE_BYTES||
       g_info.msiColor.BytesPerPixel!=2)return;
    unsigned sample=(unsigned)msi_gpu_shadow_sampler_begin(&g_gpu_shadow_auto,g_frames,
        g_vk_active,g_manual_capture_active,g_endframe_depth||g_guest_callback_depth);
    unsigned bypass=!g_manual_capture_active&&!sample&&g_vk_active&&!g_endframe_depth&&!g_guest_callback_depth&&
        !g_gpu_shadow_auto.disabled&&g_gpu_shadow_auto.passed>=GPU_SHADOW_SAMPLE_LIMIT&&
        !g_gpu_bypass.disabled&&g_gpu_bypass.attempted<12u&&g_gpu_bypass.frames<36u&&g_frames&&
        g_frames%GPU_SHADOW_SAMPLE_INTERVAL==0u&&g_gpu_bypass.last_frame!=g_frames;
    if(!g_manual_capture_active&&!sample&&!bypass)return;
    if(bypass){g_gpu_bypass.frames++;g_gpu_bypass.last_frame=g_frames;}
    unsigned slots=bypass?1u:2u;
    unsigned raw_bytes=bypass?GPU_CAPTURE_PILOT_RAW_BYTES:GPU_CAPTURE_RAW_BYTES;
    unsigned decoded_count=bypass?GPU_CAPTURE_PILOT_TEXELS:GPU_CAPTURE_DECODED_COUNT;
    MSI_INT64 alloc_begin=bypass?gpu_cost_now():0;
    unsigned pool_result=bypass?msi_gpu_pilot_storage_acquire(&g_gpu_pilot_storage,alloc,gpu_pilot_free):0;
    if(bypass){
        if(!pool_result){
            g_gpu_bypass.disabled=1;g_gpu_bypass.empty++;
            gpu_capture_release();logtext("GPU_CAPTURE pilot storage allocation failed");return;
        }
        if(pool_result==1u)g_gpu_pilot_allocations++;
        else g_gpu_pilot_reuses++;
    }
    for(unsigned i=0;i<slots;i++){
        MsiGpuCaptureSlot *slot=&g_gpu_capture_slots[i];
        slot->before=bypass?g_gpu_pilot_storage.before:(BYTE*)alloc(GPU_CAPTURE_NATIVE_BYTES);
        if(!bypass){
            slot->after=(BYTE*)alloc(GPU_CAPTURE_NATIVE_BYTES);
            slot->written=(BYTE*)alloc(GPU_CAPTURE_NATIVE_BYTES/2u);
        }
        slot->batch.snapshots.raw=bypass?g_gpu_pilot_storage.raw:(BYTE*)alloc(raw_bytes);
        slot->batch.snapshots.decoded=bypass?g_gpu_pilot_storage.decoded:(uint32_t*)alloc(decoded_count*4u);
        if(!slot->before||(!bypass&&(!slot->after||!slot->written))||
           !slot->batch.snapshots.raw||!slot->batch.snapshots.decoded){
            if(sample)msi_gpu_shadow_sampler_finish(&g_gpu_shadow_auto,MSI_SHADOW_ERROR);
            if(bypass){g_gpu_bypass.disabled=1;g_gpu_bypass.empty++;}
            gpu_capture_release();logtext("GPU_CAPTURE allocation failed");
            return;
        }
        slot->batch.snapshots.raw_capacity=raw_bytes;
        slot->batch.snapshots.decoded_capacity=decoded_count;
    }
    if(bypass)g_gpu_pilot_alloc_ticks=gpu_cost_elapsed(alloc_begin);
    g_gpu_capture_active=1;g_gpu_capture_sample=sample;g_gpu_capture_bypass=bypass;
}
static void gpu_capture_copy(BYTE *out,const BYTE *src){
    msi_gpu_native_copy(out,src);
}
static void gpu_capture_barrier(void){
    if(!g_gpu_capture_active)return;
    g_gpu_capture_draw=0;
    if(g_gpu_capture_bypass){msi_gpu_draw_batch_reset(&g_gpu_capture_batch);g_gpu_pilot_snapshot_ticks=UINT32_MAX;return;}
    if(g_gpu_capture_batch.count>=8u){
        const WORD *before=(const WORD*)g_gpu_capture_before,*after=(const WORD*)g_gpu_capture_after;
        unsigned changed=0;
        for(unsigned i=0;i<GPU_CAPTURE_NATIVE_BYTES/2u;i++)changed+=before[i]!=after[i];
        g_gpu_capture_waves++;
        /* Retain the wave with greatest net visible change, not the first
         * nonempty wave. Swap owned slots so no guest pointers or stale
         * framebuffer data survive; equal scores keep the earlier wave. */
        if(changed>g_gpu_capture_best_changed){
            MsiGpuCaptureSlot *old=g_gpu_capture_best;
            g_gpu_capture_best=g_gpu_capture_current;g_gpu_capture_current=old;
            g_gpu_capture_best_changed=changed;
        }
    }
    msi_gpu_draw_batch_reset(&g_gpu_capture_batch);
    gpu_capture_clear_loop(g_gpu_capture_current);
}
static void gpu_capture_candidate(const T_msiVertex *a,const T_msiVertex *b,const T_msiVertex *c,
    int left,int top,int right,int bottom,int width,int height,
    int textured,int modulate,int perspective,int bilinear,unsigned planes,unsigned opacity,
    unsigned tw,unsigned th,unsigned texoff,const BYTE *texture,const BYTE *lut){
    if(!g_gpu_capture_active)return;
    if(g_zplane_draw_context||width!=640||height!=400||!textured||planes!=8u||opacity!=100u||
       !texture||!lut||(!perspective&&!bilinear)||(modulate&&g_rgb888_lighting_enabled)){
        gpu_capture_barrier();return;
    }
    if(g_gpu_capture_bypass&&(!perspective||!bilinear||g_state.msiDepth.Enable||g_state.msiDepth.Protect||
       g_state.msiColor.Protect||g_state.msiTexture.Decal||g_endframe_depth||g_guest_callback_depth||
       (unsigned)(right-left)*(unsigned)(bottom-top)<16384u)){
        gpu_capture_barrier();return;
    }
    /* Even a guest write without an observed lock/remap cannot silently
       contaminate the reference. Compare current native color with the
       previous accepted draw's exact CPU output before extending the wave. */
    if(g_gpu_capture_batch.count&&(g_gpu_capture_bypass||
       !msi_gpu_snapshot_equal(g_gpu_capture_after,(const BYTE*)g_color,GPU_CAPTURE_NATIVE_BYTES)))
        gpu_capture_barrier();
    MSI_INT64 snapshot_begin=g_gpu_capture_bypass?gpu_cost_now():0;
    MsiGpuVertex vertices[3];
    const T_msiVertex *in[3]={a,b,c};
    for(unsigned i=0;i<3;i++){
        vertices[i].xy[0]=in[i]->x;vertices[i].xy[1]=in[i]->y;
        vertices[i].uv[0]=in[i]->u;vertices[i].uv[1]=in[i]->v;
        vertices[i].iw=in[i]->invW;
        vertices[i].m[0]=in[i]->mr;vertices[i].m[1]=in[i]->mg;vertices[i].m[2]=in[i]->mb;
    }
    MsiGpuTexture state={tw,th,8u,(unsigned)g_state.msiTexture.Transparency,
        (unsigned)g_state.msiTexture.KeyingColor,(unsigned)g_state.msiTexture.KeyingColorMask,
        (unsigned)g_index_key_enabled,(unsigned)g_world_blue_key_enabled,
        (unsigned)(g_icon01_opaque_enabled&&texoff==g_icon01_cache_offset)};
    unsigned flags=(perspective?1u:0u)|(g_state.msiTexture.Clamp_u?2u:0u)|
        (g_state.msiTexture.Clamp_v?4u:0u)|(bilinear?8u:0u)|(modulate?16u:0u)|
        (modulate&&g_msi_dither_enabled&&g_state.msiColor.Dither?32u:0u)|(state.transparent?64u:0u);
    MsiGpuRect rect={(unsigned)left,(unsigned)top,(unsigned)right,(unsigned)bottom};
    int rc=msi_gpu_draw_batch_push(&g_gpu_capture_batch,vertices,rect,flags,&state,texture,tw*th,lut,512u);
    if(rc!=MSI_GPU_DRAW_ACCEPT){
        gpu_capture_barrier();
        if(rc==MSI_GPU_DRAW_INVALID)return;
        rc=msi_gpu_draw_batch_push(&g_gpu_capture_batch,vertices,rect,flags,&state,texture,tw*th,lut,512u);
        if(rc!=MSI_GPU_DRAW_ACCEPT)return;
    }
    if(g_gpu_capture_batch.count==1u){
        gpu_capture_copy(g_gpu_capture_before,(const BYTE*)g_color);
        if(!g_gpu_capture_bypass)zero(g_gpu_capture_current->written,GPU_CAPTURE_NATIVE_BYTES/2u);
    }
    g_gpu_capture_draw=1;
    if(g_gpu_capture_bypass)g_gpu_pilot_snapshot_ticks=gpu_cost_elapsed(snapshot_begin);
}
static int gpu_capture_publish_wave(void);
static void gpu_capture_draw_complete(void){
    if(!g_gpu_capture_draw)return;
    if(g_gpu_capture_bypass){gpu_capture_barrier();return;}
    g_gpu_capture_draw=0;
    gpu_capture_copy(g_gpu_capture_after,(const BYTE*)g_color);
    /* Do not retain a fully discarded or completely overwritten wave. */
    if(g_gpu_capture_batch.count>=32u){
        if(gpu_capture_publish_wave())return;
        gpu_capture_barrier();
    }
}

/* GDI presents the original CPU RGB565 framebuffer plus experimental
 * software-rasterized MSI triangles. No exclusive display-mode change. */
static void vk_report(const char *message,int code){
    if(!g_log)return;
    char buf[380];DWORD n=0;
    putstr(buf,&n,"PREVIEW_VULKAN ");putstr(buf,&n,message);
    putstr(buf,&n," code=");puthex(buf,&n,(unsigned)code);
    buf[n]=0;logtext(buf);
}
static void vk_stop(void){
    matrox_vk_close();g_vk_active=0;
}
static void preview_blit(HDC dc){
    if(!g_initialized || !g_color || !dc || !g_info.msiColor.pMem)return;
    if(g_info.msiColor.BytesPerPixel!=2 || g_info.msiColor.Pitch<=0)return;
    unsigned w=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned h=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!w||!h||w>4096||h>4096)return;
    RECT rc;
    if(!GetClientRect(g_preview,&rc))return;
    int cw=rc.right-rc.left, ch=rc.bottom-rc.top;
    if(cw<=0||ch<=0)return;
    /* The original 640x400 Matrox framebuffer uses non-square source pixels.
     * Present at the CRT-correct 4:3 display aspect, not raw 8:5 sample ratio.
     * Only the GDI presentation rectangle changes: original game framebuffer,
     * vertices and OEM Win32 input/callbacks are untouched. */
    int dstw=cw,dsth=(cw*3)/4;
    if(dsth>ch){dsth=ch;dstw=(ch*4)/3;}
    if(dstw<1 || dsth<1)return;
    int dx=(cw-dstw)/2,dy=(ch-dsth)/2;
    /* Clear pillar/letterbox bars in windowed and borderless preview. */
    if(dx>0){PatBlt(dc,0,0,dx,ch,BLACKNESS);PatBlt(dc,dx+dstw,0,cw-dx-dstw,ch,BLACKNESS);}
    if(dy>0){PatBlt(dc,dx,0,dstw,dy,BLACKNESS);PatBlt(dc,dx,dy+dsth,dstw,ch-dy-dsth,BLACKNESS);}
    if(!g_viewport_reported){
        char buf[256];DWORD n=0;
        putstr(buf,&n,"PREVIEW_VIEWPORT display_aspect=4:3 source=");
        putunsigned(buf,&n,w);putstr(buf,&n,"x");putunsigned(buf,&n,h);
        putstr(buf,&n," viewport=");putunsigned(buf,&n,(unsigned)dstw);
        putstr(buf,&n,"x");putunsigned(buf,&n,(unsigned)dsth);
        putstr(buf,&n," offset=");putunsigned(buf,&n,(unsigned)dx);
        putstr(buf,&n,",");putunsigned(buf,&n,(unsigned)dy);
        buf[n]=0;logtext(buf);g_viewport_reported=1;
    }
    BITMAPINFO565 bi;
    zero(&bi,sizeof(bi));
    bi.bmi.biSize=sizeof(BITMAPINFOHEADER_MIN);
    bi.bmi.biWidth=(LONG)w;bi.bmi.biHeight=-(LONG)h;
    bi.bmi.biPlanes=1;bi.bmi.biBitCount=16;
    bi.bmi.biCompression=BI_BITFIELDS;
    bi.bmi.biSizeImage=(DWORD)(w*h*2u);
    bi.masks[0]=0xF800u;bi.masks[1]=0x07E0u;bi.masks[2]=0x001Fu;
    /* Only presentation: never write to g_color, change MSI geometry,
     * mouse mapping, native dimensions or F12 diagnostic layers.
     * Restore borrowed DC state even on error; retry the original blit
     * if the platform rejects the optional HALFTONE operation. */
    int result=0;
    int scaled=dstw!=(int)w || dsth!=(int)h;
    int smooth=g_presentation_smooth_enabled && !g_presentation_smooth_failed && scaled;
    if(smooth){
        int oldmode=SetStretchBltMode(dc,HALFTONE);
        POINT oldorigin;
        if(oldmode){
            if(SetBrushOrgEx(dc,0,0,&oldorigin)){
                result=StretchDIBits(dc,dx,dy,dstw,dsth,0,0,(int)w,(int)h,g_color,&bi,DIB_RGB_COLORS,SRCCOPY);
                SetBrushOrgEx(dc,oldorigin.x,oldorigin.y,NULL);
            }
            SetStretchBltMode(dc,oldmode);
        }
        if(result!=0 && (unsigned)result!=GDI_ERROR){
            if(!g_presentation_smooth_reported){
                logtext("PREVIEW_SCALER HALFTONE blit succeeded (presentation only)");
                g_presentation_smooth_reported=1;
            }
        }else{
            g_presentation_smooth_failed=1;
            logtext("PREVIEW_SCALER HALFTONE unavailable or blit failed; using legacy scaling for this session");
        }
    }
    if(!smooth || g_presentation_smooth_failed)
        result=StretchDIBits(dc,dx,dy,dstw,dsth,0,0,(int)w,(int)h,g_color,&bi,DIB_RGB_COLORS,SRCCOPY);
    if(result==0 || (unsigned)result==GDI_ERROR){
        g_preview_failures++;
        if(g_preview_failures<=3)logtext("PREVIEW_GDI StretchDIBits FAILED");
    }else g_preview_presented++;
}
/* The original Matrox MSI95.dll creates MSI95_Class and forwards normal
 * Win32 messages to the application-supplied WNDPROC through CallWindowProcA.
 * v0.7/v0.8 never invoked this callback: movies rendered but keyboard and
 * focus/activation notifications could not reach the actual game.
 *
 * This diagnostic preview preserves our GDI painting while forwarding
 * unhandled messages to the original callback. No synthetic keys, no
 * forced menu state and no claim of playable graphics.
 */
static LRESULT __stdcall preview_window_proc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam){
    if(msg==WM_ERASEBKGND)return 1;
    /* The renderer owns its native preview icon. The 1996 OEM WndProc
     * must not receive this housekeeping message. */
    if(msg==WM_SETICON)return DefWindowProcA(hwnd,msg,wparam,lparam);
    if(msg==WM_PAINT){
        PAINTSTRUCT_MIN ps;zero(&ps,sizeof(ps));
        HDC dc=BeginPaint(hwnd,&ps);
        if(dc){if(!g_vk_active)preview_blit(dc);EndPaint(hwnd,&ps);}
        return 0;
    }
    if(msg==WM_CLOSE){
        logtext("PREVIEW_WINDOW CLOSED by user");
        vk_stop();DestroyWindow(hwnd);return 0;
    }
    if(msg==WM_DESTROY){if(g_preview==hwnd){vk_stop();g_preview=NULL;}return 0;}
    /* Do not reenter the OEM WndProc while MSI heaps and framebuffers are torn down. */
    if(g_shutdown_started)return DefWindowProcA(hwnd,msg,wparam,lparam);
    /* The original game WNDPROC (written for the Win95 MSI full-screen
     * window) receives focus/activation messages from our different,
     * modern overlapped GDI window. On Windows 11 the user confirmed the
     * OEM engine begins its NORMAL MSI cleanup immediately after focus loss.
     * Keep native Win32 activation and nonclient behavior via DefWindowProc,
     * but do not expose the focus-only transition to the 1996 game callback.
     * We still forward all game keyboard and mouse messages. An intentional
     * WM_CLOSE still closes the preview, and msiExit is not suppressed.
     * This is a testable compatibility workaround, not a proven general fix.
     */
    if(msg==WM_KILLFOCUS||msg==WM_SETFOCUS||msg==WM_ACTIVATE||
       msg==WM_ACTIVATEAPP||msg==WM_NCACTIVATE){
        g_focus_filtered_events++;
        if(g_focus_filtered_events<=40 || (g_focus_filtered_events%100)==0){
            char buf[256];DWORD n=0;
            putstr(buf,&n,"FOCUS_ISOLATED msg=");puthex(buf,&n,msg);
            putstr(buf,&n," wparam=");puthex(buf,&n,(unsigned)wparam);
            putstr(buf,&n," not_forwarded_to_game count=");
            putunsigned(buf,&n,g_focus_filtered_events);buf[n]=0;
            logtext(buf);
        }
        return DefWindowProcA(hwnd,msg,wparam,lparam);
    }
    /* A translated Space character can be delivered after its original
     * KeyDown has already triggered a new scene. The OEM's scene-global
     * pointer is legitimately NULL during the texture-heap reload. A second
     * character in that brief window was immediately followed by the real
     * game AV 0046D76D in the 2026-11-20 native Windows session.
     * Do not swallow any other message and require the exact known OEM
     * instruction signature. This guard does not change the guest EXE.
     */
    if(msg==WM_CHAR && (unsigned)wparam==0x20u && g_initialized){
        /* Evaluate the same narrow guard, but report why the delivered
         * character was passed or suppressed in an explicit Log=1 session.
         * Do not suppress KeyDown, KeyUp, mouse, non-Space or other states. */
        DWORD age=g_last_heap_alloc_tick!=0u?
            (DWORD)(GetTickCount()-g_last_heap_alloc_tick):0xFFFFFFFFu;
        int signature_ok=game_global_probe_supported();
        int readable=signature_ok &&
            readable_span((const void*)0x0050DCD0u,sizeof(DWORD));
        DWORD game_global=readable?
            *(const volatile DWORD*)0x0050DCD0u:0xFFFFFFFFu;
        int suppress=g_heap_count>0u && g_last_heap_alloc_tick!=0u &&
            age<=250u && readable && game_global==0u;
        if(g_log && g_transition_space_char_evaluations<12u){
            char info[380];DWORD n=0;
            g_transition_space_char_evaluations++;
            putstr(info,&n,"TRANSITION_SPACE_CHAR_EVAL frame=");
            putunsigned(info,&n,g_frames);
            putstr(info,&n," heap_live=");putunsigned(info,&n,g_heap_count);
            putstr(info,&n," heap_age_ms=");putunsigned(info,&n,age);
            putstr(info,&n," scene_global=");puthex(info,&n,game_global);
            putstr(info,&n," signature_ok=");putunsigned(info,&n,(unsigned)signature_ok);
            putstr(info,&n," callback_depth=");putunsigned(info,&n,g_guest_callback_depth);
            putstr(info,&n," suppress=");putunsigned(info,&n,(unsigned)suppress);
            info[n]=0;logtext(info);
        }
        if(suppress){
            g_transition_space_char_ignored++;
            lognum("TRANSITION_SPACE_CHAR_IGNORED count=",
                   g_transition_space_char_ignored,"frame=",g_frames);
            return 0;
        }
    }
    /* Capture the exact scene the tester is inspecting, not an arbitrary
     * numbered frame. F12 is a probe-only key and never forwarded to guest. */
    if((msg==WM_KEYDOWN || msg==WM_KEYUP) && (unsigned)wparam==0x7Bu){
        if(msg==WM_KEYUP)g_f12_held=0;
        else if(!g_f12_held){
            g_f12_held=1;
            if(g_manual_capture_pending<32u)g_manual_capture_pending++;
            lognum("MANUAL_CAPTURE requested_at_frame=",g_frames,NULL,0);
        }
        return 0;
    }
    /* Guard the OEM WndProc against a mouse click delivered as soon as the
     * Wine/Win32 preview window acquires focus. The game has not yet created
     * its input table. The original splanetw.exe (hash-verified by launcher)
     * stores that pointer at VA 004BA4B8; never modify guest memory here.
     * While waiting, mouse moves and button pairs are dropped; key input,
     * focus isolation, F12 and normal Win32 window behavior are unchanged. */
    if(msg==WM_MOUSEMOVE || msg==WM_LBUTTONDOWN || msg==WM_LBUTTONUP ||
       msg==WM_RBUTTONDOWN || msg==WM_RBUTTONUP){
        int guest_input_ready=0;
        DWORD oem_table=0;
        const volatile DWORD *ptr=(const volatile DWORD*)0x004BA4B8u;
        if(readable_span((const void*)ptr,sizeof(DWORD)))oem_table=*ptr;
        guest_input_ready=(g_initialized && g_frames>=8u &&
                           g_heap_count>0u && oem_table!=0u);
        int swallowed_pair=(msg==WM_LBUTTONUP && g_swallowed_left_down) ||
                           (msg==WM_RBUTTONUP && g_swallowed_right_down);
        if(!guest_input_ready || swallowed_pair){
            if(msg==WM_LBUTTONDOWN)g_swallowed_left_down=1;
            if(msg==WM_RBUTTONDOWN)g_swallowed_right_down=1;
            if(msg==WM_LBUTTONUP)g_swallowed_left_down=0;
            if(msg==WM_RBUTTONUP)g_swallowed_right_down=0;
            g_early_mouse_ignored++;
            if(msg!=WM_MOUSEMOVE || g_early_mouse_ignored<=3u ||
               g_early_mouse_ignored%300u==0u){
                if(g_early_mouse_ignored<=30u || g_early_mouse_ignored%300u==0u){
                    char buf[320];DWORD n=0;
                    putstr(buf,&n,"EARLY_MOUSE_IGNORED msg=");puthex(buf,&n,msg);
                    putstr(buf,&n," frames=");putunsigned(buf,&n,g_frames);
                    putstr(buf,&n," heaps=");putunsigned(buf,&n,g_heap_count);
                    putstr(buf,&n," oem_input_table=");puthex(buf,&n,oem_table);
                    putstr(buf,&n," elapsed_ms=");putunsigned(buf,&n,GetTickCount()-g_boot_tick);
                    putstr(buf,&n," count=");putunsigned(buf,&n,g_early_mouse_ignored);
                    buf[n]=0;logtext(buf);
                }
            }
            return 0;
        }
        if(g_early_mouse_ready_logs==0u){
            g_early_mouse_ready_logs++;
            lognum("MOUSE_INPUT_READY frame=",g_frames," ignored_early=",g_early_mouse_ignored);
        }
    }
    if(msg==WM_LBUTTONUP){
        g_last_left_click_ms=GetTickCount();
        lognum("MOUSE_UP elapsed_ms=",g_last_left_click_ms-g_boot_tick,"frame=",g_frames);
        char coords[170];DWORD i=0;
        putstr(coords,&i,"MOUSE_POSITION x=");putunsigned(coords,&i,(unsigned)((unsigned)lparam&65535u));
        putstr(coords,&i," y=");putunsigned(coords,&i,(unsigned)(((unsigned)lparam>>16)&65535u));
        coords[i]=0;logtext(coords);
    }
    if(msg==WM_KEYDOWN||msg==WM_KEYUP||
       msg==WM_CHAR||msg==WM_SYSKEYDOWN||msg==WM_SYSKEYUP||
       msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||
       msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP){
        g_input_messages++;
        if(g_input_messages<=60 || g_input_messages%100==0){
            char buf[240];DWORD n=0;
            putstr(buf,&n,"WIN32_INPUT msg=");puthex(buf,&n,msg);
            putstr(buf,&n," wparam=");puthex(buf,&n,(unsigned)wparam);
            putstr(buf,&n," callback=");puthex(buf,&n,(unsigned)g_guest_window_proc);
            putstr(buf,&n," count=");putunsigned(buf,&n,g_input_messages);
            buf[n]=0;logtext(buf);
        }
    }
    if(g_guest_window_proc){
        LRESULT result;
        sample_game_global_50dcd0("pre_callback");
        g_guest_messages++;
        g_last_forwarded_msg=msg;
        g_last_forwarded_frame=g_frames;
        g_last_forwarded_tick=GetTickCount();
        gpu_dual_thread_observe();
        if(g_gpu_profile_active){msi_gpu_defer_barrier(&g_gpu_defer_audit);msi_gpu_defer_barrier(&g_gpu_defer_strict);}
        g_guest_callback_depth++;
        result=CallWindowProcA(g_guest_window_proc,hwnd,msg,wparam,lparam);
        sample_game_global_50dcd0("post_callback");
        g_guest_callback_depth--;
        return result;
    }
    return DefWindowProcA(hwnd,msg,wparam,lparam);
}
/* The user-owned OEM splanetw.exe contains GROUP_ICON #101 with 16x16,
 * 32x32 and 48x48 variants. We inspected that group on the original CD.
 * Use its already-installed resource: no icon files or copyrighted assets
 * are shipped with the fix. LR_SHARED icons live as long as the module.
 * Fall back to Windows stock icon only if the OEM resource is absent. */
static HANDLE game_preview_icon(int size){
    HANDLE exe=GetModuleHandleA(NULL);
    HANDLE result=exe?LoadImageA(exe,(const char*)OEM_GROUP_ICON_ID,IMAGE_ICON,size,size,LR_SHARED):NULL;
    if(!result)result=LoadImageA(NULL,(const char*)32512u,IMAGE_ICON,size,size,LR_SHARED);
    return result;
}
static void preview_open(void){
    if(g_preview_created)return;
    g_preview_created=1;
    WNDCLASSA_MIN wc;zero(&wc,sizeof(wc));
    wc.style=3;wc.lpfnWndProc=preview_window_proc;
    HANDLE icon_big=game_preview_icon(32),icon_small=game_preview_icon(16);
    wc.hIcon=icon_big;
    wc.hInstance=g_module;
    wc.lpszClassName="MSI95_Class";
    if(!g_preview_class_registered){
        if(!RegisterClassA(&wc)){logtext("PREVIEW_WINDOW: RegisterClassA failed");return;}
        g_preview_class_registered=1;
    }
    RECT rc;rc.left=0;rc.top=0;rc.right=960;rc.bottom=600;
    DWORD style=WS_OVERLAPPEDWINDOW|WS_VISIBLE;
    int x=CW_USEDEFAULT,y=CW_USEDEFAULT;
    int winw=960,winh=600;
    g_borderless_active=0;
    if(g_borderless_enabled){
        int screenw=GetSystemMetrics(SM_CXSCREEN),screenh=GetSystemMetrics(SM_CYSCREEN);
        if(screenw>=640 && screenh>=400 && screenw<=8192 && screenh<=8192){
            g_borderless_active=1;
            style=WS_POPUP|WS_VISIBLE;
            x=0;y=0;winw=screenw;winh=screenh;
        }else logtext("PREVIEW_BORDERLESS invalid desktop dimensions; falling back to 960x600 window");
    }
    if(!g_borderless_active){
        if(AdjustWindowRect(&rc,WS_OVERLAPPEDWINDOW,FALSE)){
            winw=rc.right-rc.left;winh=rc.bottom-rc.top;
        }else{winw=960;winh=640;}
    }
    g_preview=CreateWindowExA(0,wc.lpszClassName,
        "Scorched Planet Matrox - MSI95 software renderer (experimental)",
        style,x,y,winw,winh,NULL,NULL,g_module,NULL);
    if(!g_preview){logtext("PREVIEW_WINDOW: CreateWindowExA failed");return;}
    /* Send both sizes before showing the borderless popup: Wine can then
     * propagate _NET_WM_ICON to XFCE and Windows gets the taskbar icon. */
    if(icon_big)SendMessageA(g_preview,WM_SETICON,ICON_BIG,(LPARAM)icon_big);
    if(icon_small)SendMessageA(g_preview,WM_SETICON,ICON_SMALL,(LPARAM)icon_small);
    {char message[150];DWORD n=0;
      putstr(message,&n,"PREVIEW_WINDOW_ICON OEM_group=101 big_present=");
      putunsigned(message,&n,(unsigned)(icon_big!=NULL));
      putstr(message,&n," small_present=");putunsigned(message,&n,(unsigned)(icon_small!=NULL));
      message[n]=0;logtext(message);
    }
    ShowWindow(g_preview,1);UpdateWindow(g_preview);
    {char buf[220];DWORD n=0;
      putstr(buf,&n,"PREVIEW_WINDOW CREATED mode=");
      putstr(buf,&n,g_borderless_active?"borderless-desktop":"windowed");
      putstr(buf,&n," window_width=");putunsigned(buf,&n,(unsigned)winw);
      putstr(buf,&n," window_height=");putunsigned(buf,&n,(unsigned)winh);
      putstr(buf,&n," source=640x400 display_aspect=4:3 pixel_aspect=5:6 pillarbox=black; RGB565, input forwarded");
      buf[n]=0;logtext(buf);
    }
}
static void preview_present(void){
    if(!g_preview)return;
    DWORD preview_tick=GetTickCount();
    MSG_MIN msg;
    unsigned pumps=0;
    /* A game callback may itself render one or more MSI95 frames. Pumping
     * Win32 events again during those frames recursively calls the original
     * OEM WndProc before the previous event has finished: observed
     * endframe_depth=2, guest_callback_depth=3 at briefing AV 0048585B.
     * Leave all pending messages untouched until the outer game frame.
     * Framebuffer presentation remains unchanged even on nested calls. */
    if(g_endframe_depth==1u && g_guest_callback_depth==0u){
        while(pumps<32 && g_preview && PeekMessageA(&msg,g_preview,0,0,PM_REMOVE)){
            TranslateMessage(&msg);DispatchMessageA(&msg);pumps++;
        }
    }else{
        g_inner_input_pumps_deferred++;
        if(g_inner_input_pumps_deferred<=8u ||
           g_inner_input_pumps_deferred%100u==0u){
            lognum("NESTED_INPUT_PUMP_DEFERRED count=",
                   g_inner_input_pumps_deferred,"endframe_depth=",g_endframe_depth);
            lognum("NESTED_INPUT_PUMP_CONTEXT callback_depth=",
                   g_guest_callback_depth,"frame=",g_frames);
        }
    }
    if(!g_preview)return;
    /* Initialize lazily on the rendering thread, after window creation and
     * outside the guest message callback. Never draw GDI into a live Vulkan
     * swapchain. Failed initialization or presentation keeps native game
     * memory intact and permanently falls back to GDI for this session. */
    if(g_vk_requested&&!g_vk_attempted&&g_endframe_depth==1u&&g_guest_callback_depth==0u){
        g_vk_attempted=1;
        if(g_info.msiColor.BytesPerPixel==2&&g_info.msiColor.Pitch==1280&&g_info.msiColor.MemSize==512000){
            int status=matrox_vk_open(g_preview,g_presentation_smooth_enabled,vk_report);
            g_vk_active=(status==0);
            if(g_vk_active)logtext("PREVIEW_RENDERER active=vulkan (CPU raster + GPU presentation)");
            else vk_report("initialization failed; active=software",status);
        }else logtext("PREVIEW_VULKAN unsupported native framebuffer; active=software");
    }
    if(g_vk_active){
        DWORD t=GetTickCount();
        int status=matrox_vk_present((const WORD*)g_color,(DWORD)g_info.msiColor.Pitch);
        g_vk_ms+=GetTickCount()-t;
        if(status==0){g_vk_frames++;g_preview_presented++;g_vk_retry_streak=0;}
        else if(status==2){g_vk_skipped++;}
        else if(status==3&&g_vk_retry_streak<2u){
            vk_stop();g_vk_recreates++;g_vk_retry_streak++;g_vk_attempted=0;
            lognum("PREVIEW_VULKAN recreate requested count=",g_vk_recreates,NULL,0);
        }else{vk_stop();logtext("PREVIEW_RENDERER Vulkan present failed; active=software for this session");}
    }
    if(!g_vk_active){
        HDC dc=GetDC(g_preview);
        if(dc){preview_blit(dc);ReleaseDC(g_preview,dc);}
    }
    g_preview_ms += GetTickCount()-preview_tick;
    if(g_frames==5||g_frames==60||g_frames==240||g_frames%600==0){
        lognum("PREVIEW_PRESENT frames=",g_frames,"successful_blits=",g_preview_presented);
        if(g_vk_requested){lognum("PREVIEW_VULKAN frames=",g_vk_frames,"skipped=",g_vk_skipped);lognum("PREVIEW_VULKAN present_ms=",g_vk_ms,"active=",g_vk_active); }
        lognum("GAME_WINDOW_EVENTS forwarded=",g_guest_messages,"input_msgs=",g_input_messages);
        lognum("FOCUS_FILTER_STATS ignored_for_guest=",g_focus_filtered_events,NULL,0);
        lognum("INPUT_PUMP_STATS nested_deferred=",g_inner_input_pumps_deferred,
               "frame=",g_frames);
    }
}
static void clean(void){
    gpu_capture_release();
    gpu_pilot_storage_destroy();
    vk_stop();
    for(unsigned i=0;i<g_heap_count;i++)if(g_heaps[i])VirtualFree(g_heaps[i],0,MEM_RELEASE);
    if(g_heaps)VirtualFree(g_heaps,0,MEM_RELEASE);
    g_heaps=NULL;
    g_heap_count=0;g_heap_capacity=0;g_heap_peak=0;
    if(g_color_guest&&g_color_guest!=g_color){
        BOOL guest_ok=UnmapViewOfFile(g_color_guest);
        BOOL engine_ok=UnmapViewOfFile(g_color);
        lognum("GPU_DUAL_VIEW_RELEASE success=",guest_ok&&engine_ok,NULL,0);
    }else if(g_color)VirtualFree(g_color,0,MEM_RELEASE);
    g_color_guest=NULL;
    if(g_depth)VirtualFree(g_depth,0,MEM_RELEASE);
    if(g_texture)VirtualFree(g_texture,0,MEM_RELEASE);
    if(g_ownership)VirtualFree(g_ownership,0,MEM_RELEASE);
    if(g_atlas_provenance)VirtualFree(g_atlas_provenance,0,MEM_RELEASE);
    if(g_world_provenance)VirtualFree(g_world_provenance,0,MEM_RELEASE);
    if(g_shadow_provenance)VirtualFree(g_shadow_provenance,0,MEM_RELEASE);
    if(g_final_texel_source)VirtualFree(g_final_texel_source,0,MEM_RELEASE);
    if(g_final_source_audit)VirtualFree(g_final_source_audit,0,MEM_RELEASE);
    if(g_undithered_reference)VirtualFree(g_undithered_reference,0,MEM_RELEASE);
    if(g_final_light_inputs)VirtualFree(g_final_light_inputs,0,MEM_RELEASE);
    if(g_shadow_underlay)VirtualFree(g_shadow_underlay,0,MEM_RELEASE);
    g_shadow_underlay=NULL;
    g_ownership=NULL;g_atlas_provenance=NULL;g_world_provenance=NULL;
    g_shadow_provenance=NULL;g_final_texel_source=NULL;g_final_source_audit=NULL;
    g_undithered_reference=NULL;g_final_light_inputs=NULL;
    g_ownership_bytes=0;g_capture_ownership=0;
    g_color=g_depth=g_texture=NULL;g_initialized=0;
    g_state_valid=0;
    zero(&g_state,sizeof(g_state));
    zero(g_texture_uploaded,sizeof(g_texture_uploaded));
    if(g_preview){DestroyWindow(g_preview);g_preview=NULL;}
    g_preview_created=0;g_borderless_active=0;g_viewport_reported=0;
}

/* Numbered manual captures live beside the DLL under game/. The two
 * related filenames use one serial and contain a raw frame and ownership
 * map from the SAME render frame. The sequence scans existing files so
 * numbers survive program restarts and a partial/failed previous pair.
 * The final CreateFile uses CREATE_NEW even after the existence probe:
 * no race or manual collision can ever truncate an existing picture. */
static void capture_name(char*dst,unsigned serial,const char*suffix){
    DWORD n=0;
    putstr(dst,&n,"matrox_capture_");
    if(serial<1000u)dst[n++]='0';
    if(serial<100u)dst[n++]='0';
    if(serial<10u)dst[n++]='0';
    putunsigned(dst,&n,serial);
    putstr(dst,&n,suffix);
    dst[n]=0;
}
static int capture_file_present(const char*filename){
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(!n||n>=1023)return -1;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;
    while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}
    if(filename[k])return -1;
    path[i+k]=0;
    if(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES)return 1;
    DWORD error=GetLastError();
    if(error==2u||error==3u)return 0; /* FILE_NOT_FOUND / PATH_NOT_FOUND */
    lognum("MANUAL_CAPTURE file attribute error=",error,NULL,0);return -1;
}
static int next_capture_pair(char*frame,char*coverage,char*atlas,char*world,char*shadow,char*texel,char*audit,char*undithered,char*lighting){
    for(unsigned tries=0;tries<999999u;tries++){
        if(g_manual_capture_sequence>=999999u)g_manual_capture_sequence=0;
        unsigned serial=++g_manual_capture_sequence;
        capture_name(frame,serial,"_frame.bmp");
        capture_name(coverage,serial,"_coverage.bmp");
        capture_name(atlas,serial,"_atlas.bmp");
        capture_name(world,serial,"_world.bmp");
        capture_name(shadow,serial,"_shadow.bmp");
        capture_name(texel,serial,"_texel.bmp");
        capture_name(audit,serial,"_sourceaudit.bmp");
        capture_name(undithered,serial,"_nodither.bmp");
        capture_name(lighting,serial,"_lighting.bin");
        int frame_exists=capture_file_present(frame);
        int coverage_exists=capture_file_present(coverage);
        int atlas_exists=capture_file_present(atlas);
        int world_exists=capture_file_present(world);
        int shadow_exists=capture_file_present(shadow);
        int texel_exists=capture_file_present(texel);
        int audit_exists=capture_file_present(audit);
        int nodither_exists=capture_file_present(undithered);
        int lighting_exists=capture_file_present(lighting);
        char gpu[100];capture_name(gpu,serial,"_gpu.bin");
        int gpu_exists=capture_file_present(gpu);
        if(frame_exists<0||coverage_exists<0||atlas_exists<0||world_exists<0||shadow_exists<0||texel_exists<0||audit_exists<0||nodither_exists<0||lighting_exists<0||gpu_exists<0){
            logtext("MANUAL_CAPTURE cannot resolve output directory");
            return FALSE;
        }
        if(!frame_exists&&!coverage_exists&&!atlas_exists&&!world_exists&&!shadow_exists&&!texel_exists&&!audit_exists&&!nodither_exists&&!lighting_exists&&!gpu_exists)return TRUE;
    }
    logtext("MANUAL_CAPTURE exhausted available file numbers");
    return FALSE;
}
/* Snapshot the combined CPU + experimental MSI RGB565 framebuffer.
 * BMP uses top-down BI_BITFIELDS. */
static void put16le(BYTE *p,unsigned v){p[0]=(BYTE)v;p[1]=(BYTE)(v>>8);}
static void put32le(BYTE *p,unsigned v){put16le(p,v);put16le(p+2,v>>16);}
/* M95GPU01: little-endian Win32 fixed records, no native pointers.
 * header: magic, width,height,draws,textures,decoded_words,frame (32 bytes).
 * texture records: width,height,decoded word offset. Then draw records
 * (id,flags,x0,y0,x1,y1), 3 float32 vertices per draw, R32_UINT texels,
 * and before/after RGB565 native framebuffers. Bounded to <10 MiB. */
static BOOL gpu_capture_write(HANDLE h,const void *p,DWORD bytes){
    DWORD written=0;return WriteFile(h,p,bytes,&written,NULL)&&written==bytes;
}
/* Exercise the exact retirement helper on a PRIVATE copy of the wave.
 * No guest access contract assumed here; never publish to live memory.
 * The simulated failure does not invalidate the real Vulkan device. */
static int gpu_capture_retirement_check(const MsiGpuCaptureSlot *best,
    const uint32_t *tagged,const MsiGpuTaggedCheck *check,WORD *private_color,WORD *cpu){
    const unsigned pixels=GPU_CAPTURE_NATIVE_BYTES/2u;
    BYTE *under=(BYTE*)alloc(pixels),*mask=(BYTE*)alloc(pixels);
    if(!under||!mask){
        if(under)VirtualFree(under,0,MEM_RELEASE);
        if(mask)VirtualFree(mask,0,MEM_RELEASE);
        return MSI_SHADOW_ERROR;
    }
    const WORD *before=(const WORD*)best->before,*after=(const WORD*)best->after;
    unsigned eligible=check->homogeneous;
    for(unsigned k=0;k<best->batch.count;k++)
        eligible&=(best->batch.draws[k].flags&9u)==9u;
    unsigned errors=0,gpu_writes=0,cpu_writes=0;
    DWORD started=GetTickCount();
    MsiGpuBypass result;
    if(eligible){
        for(unsigned j=0;j<pixels;j++){private_color[j]=before[j];under[j]=3u;}
        int outcome=msi_gpu_deferred_finish(&result,&best->batch,640u,400u,
            private_color,under,before,1,tagged,cpu,mask);
        errors+=outcome!=MSI_DEFER_FINISH_GPU;
        gpu_writes=result.written;
        errors+=gpu_writes!=check->written;
        unsigned key=(best->batch.draws[0].flags&64u)?2u:1u;
        for(unsigned j=0;j<pixels;j++){
            unsigned write=tagged[j]!=65536u;
            errors+=private_color[j]!=(write?(WORD)tagged[j]:before[j])||
                    under[j]!=(write?key:3u);
        }
    }
    for(unsigned j=0;j<pixels;j++){private_color[j]=before[j];under[j]=3u;}
    int outcome=msi_gpu_deferred_finish(&result,&best->batch,640u,400u,
        private_color,under,before,0,NULL,cpu,mask);
    errors+=outcome!=MSI_DEFER_FINISH_CPU;
    cpu_writes=result.written;
    unsigned expected_writes=0;
    for(unsigned j=0;j<pixels;j++){
        expected_writes+=best->written[j]!=0u;
        errors+=private_color[j]!=after[j]||
                under[j]!=(best->written[j]?best->written[j]:3u);
    }
    errors+=cpu_writes!=expected_writes;
    VirtualFree(mask,0,MEM_RELEASE);VirtualFree(under,0,MEM_RELEASE);
    char msg[300];DWORD n=0;
    putstr(msg,&n,errors?"GPU_RETIREMENT DIFFERENCES frame=":"GPU_RETIREMENT PASS frame=");
    putunsigned(msg,&n,g_frames);putstr(msg,&n," draws=");putunsigned(msg,&n,best->batch.count);
    putstr(msg,&n," gpu_tested=");putunsigned(msg,&n,eligible);
    putstr(msg,&n," gpu_written=");putunsigned(msg,&n,gpu_writes);
    putstr(msg,&n," cpu_written=");putunsigned(msg,&n,cpu_writes);
    putstr(msg,&n," errors=");putunsigned(msg,&n,errors);
    putstr(msg,&n," elapsed_ms=");putunsigned(msg,&n,GetTickCount()-started);
    msg[n]=0;logtext(msg);
    return errors?MSI_SHADOW_DIFFERENCES:MSI_SHADOW_PASS;
}
/* 1 pass, 2 differences, -1 error, 0 skipped. */
static int gpu_capture_shadow_compare(const MsiGpuCaptureSlot *best,unsigned expected_changed,int publish){
    if(!g_vk_active){logtext("GPU_SHADOW SKIP presentation unavailable");return 0;}
    if(g_endframe_depth!=(publish?0u:1u)||g_guest_callback_depth){logtext("GPU_SHADOW SKIP nested frame");return 0;}
    WORD *scratch=(WORD*)alloc(GPU_CAPTURE_NATIVE_BYTES);
    if(!scratch){logtext("GPU_SHADOW SKIP scratch allocation failed");return -1;}
    /* Validate recovery against the original rasterizer, not against
     * the GPU tolerance. Never touch the live framebuffer during replay. */
    BYTE *replayed_mask=(BYTE*)alloc(GPU_CAPTURE_NATIVE_BYTES/2u);
    DWORD replay_started=GetTickCount();
    if(!replayed_mask){
        VirtualFree(scratch,0,MEM_RELEASE);logtext("GPU_CPU_REPLAY ERROR allocation");
        return MSI_SHADOW_ERROR;
    }
    MSI_INT64 replay_begin=gpu_cost_now();
    int replay_ok=msi_gpu_cpu_replay(&best->batch,640u,400u,
        (const WORD*)best->before,scratch,replayed_mask);
    DWORD replay_ticks=gpu_cost_elapsed(replay_begin);
    unsigned exact=0,mask_diff=0,written=0,first=GPU_CAPTURE_NATIVE_BYTES/2u;
    if(replay_ok)for(unsigned j=0;j<GPU_CAPTURE_NATIVE_BYTES/2u;j++){
        unsigned same=scratch[j]==((const WORD*)best->after)[j];
        unsigned same_mask=replayed_mask[j]==best->written[j];
        exact+=same;mask_diff+=!same_mask;written+=replayed_mask[j]!=0;
        if((!same||!same_mask)&&first==GPU_CAPTURE_NATIVE_BYTES/2u)first=j;
    }
    int replay_pass=replay_ok&&exact==GPU_CAPTURE_NATIVE_BYTES/2u&&!mask_diff&&written;
    char replay_msg[320];DWORD rn=0;
    putstr(replay_msg,&rn,replay_pass?"GPU_CPU_REPLAY PASS frame=":"GPU_CPU_REPLAY DIFFERENCES frame=");
    putunsigned(replay_msg,&rn,g_frames);
    putstr(replay_msg,&rn," draws=");putunsigned(replay_msg,&rn,best->batch.count);
    putstr(replay_msg,&rn," exact=");putunsigned(replay_msg,&rn,exact);
    putstr(replay_msg,&rn," mask_diff=");putunsigned(replay_msg,&rn,mask_diff);
    putstr(replay_msg,&rn," written=");putunsigned(replay_msg,&rn,written);
    putstr(replay_msg,&rn," elapsed_ms=");putunsigned(replay_msg,&rn,GetTickCount()-replay_started);
    replay_msg[rn]=0;logtext(replay_msg);
    /* Match only the selected real CPU wave; loop timing excludes state
     * preparation and capture copies, but includes diagnostic mask writes.
     * Replay time includes private baseline copy/mask clear. */
    if(replay_pass){
        unsigned valid=g_cap_frequency&&best->loop_draws==best->batch.count&&
            !best->loop_untimed&&replay_ticks!=UINT32_MAX;
        rn=0;putstr(replay_msg,&rn,"GPU_CPU_LOOP frame=");putunsigned(replay_msg,&rn,g_frames);
        putstr(replay_msg,&rn," draws=");putunsigned(replay_msg,&rn,best->batch.count);
        putstr(replay_msg,&rn," timed_draws=");putunsigned(replay_msg,&rn,best->loop_draws-best->loop_untimed);
        putstr(replay_msg,&rn," qpc_hz=");putunsigned(replay_msg,&rn,valid?g_cap_frequency:0);
        putstr(replay_msg,&rn," loop_ticks=");putunsigned(replay_msg,&rn,valid?best->loop_ticks:0);
        putstr(replay_msg,&rn," replay_ticks=");putunsigned(replay_msg,&rn,valid?replay_ticks:0);
        replay_msg[rn]=0;logtext(replay_msg);
    }
    if(!replay_pass&&first<GPU_CAPTURE_NATIVE_BYTES/2u){
        rn=0;putstr(replay_msg,&rn,"GPU_CPU_REPLAY MISMATCH x=");putunsigned(replay_msg,&rn,first%640u);
        putstr(replay_msg,&rn," y=");putunsigned(replay_msg,&rn,first/640u);
        putstr(replay_msg,&rn," expected=");puthex(replay_msg,&rn,((const WORD*)best->after)[first]);
        putstr(replay_msg,&rn," replay=");puthex(replay_msg,&rn,scratch[first]);
        putstr(replay_msg,&rn," expected_mask=");putunsigned(replay_msg,&rn,best->written[first]);
        putstr(replay_msg,&rn," replay_mask=");putunsigned(replay_msg,&rn,replayed_mask[first]);
        replay_msg[rn]=0;logtext(replay_msg);
    }
    VirtualFree(replayed_mask,0,MEM_RELEASE);
    if(!replay_pass){VirtualFree(scratch,0,MEM_RELEASE);return replay_ok?2:MSI_SHADOW_ERROR;}
    MsiGpuCompare stats;DWORD started=GetTickCount();
    int rc=matrox_vk_shadow_compare(&best->batch,(const WORD*)best->before,(const WORD*)best->after,scratch,&stats);
    char msg[400];DWORD n=0;
    if(rc){
        VirtualFree(scratch,0,MEM_RELEASE);
        putstr(msg,&n,"GPU_SHADOW ERROR code=");puthex(msg,&n,(unsigned)rc);
        putstr(msg,&n,g_gpu_capture_sample?" scope=auto":" scope=F12");msg[n]=0;logtext(msg);return -1;
    }
    int pass=msi_gpu_compare_pass(&stats,GPU_CAPTURE_NATIVE_BYTES/2u)&&stats.cpu_changed==expected_changed;
    putstr(msg,&n,pass?"GPU_SHADOW PASS frame=":"GPU_SHADOW DIFFERENCES frame=");
    putunsigned(msg,&n,g_frames);putstr(msg,&n," draws=");putunsigned(msg,&n,best->batch.count);
    putstr(msg,&n," active=");putunsigned(msg,&n,stats.active);
    putstr(msg,&n," exact=");putunsigned(msg,&n,stats.active_exact);
    putstr(msg,&n," one_lsb=");putunsigned(msg,&n,stats.one);
    putstr(msg,&n," edge=");putunsigned(msg,&n,stats.edge);
    putstr(msg,&n," unexplained=");putunsigned(msg,&n,stats.bad);
    putstr(msg,&n," outside_bounds=");putunsigned(msg,&n,stats.outside);
    putstr(msg,&n," cpu_only=");putunsigned(msg,&n,stats.cpu_only);
    putstr(msg,&n," gpu_only=");putunsigned(msg,&n,stats.gpu_only);
    putstr(msg,&n," elapsed_ms=");putunsigned(msg,&n,GetTickCount()-started);
    putstr(msg,&n,publish?" scope=pilot":g_gpu_capture_sample?" scope=auto":" scope=F12");
    putstr(msg,&n," textures=");putunsigned(msg,&n,best->batch.snapshots.count);
    msg[n]=0;logtext(msg);
    for(unsigned j=0;j<stats.bad&&j<MSI_GPU_COMPARE_EXAMPLES;j++){
        const MsiGpuCompareExample *e=&stats.examples[j];n=0;
        putstr(msg,&n,"GPU_SHADOW MISMATCH x=");putunsigned(msg,&n,e->x);
        putstr(msg,&n," y=");putunsigned(msg,&n,e->y);
        putstr(msg,&n," cpu=");puthex(msg,&n,e->expected);
        putstr(msg,&n," gpu=");puthex(msg,&n,e->actual);msg[n]=0;logtext(msg);
    }
    if(pass){
        /* A second replay on inverted initial colors detects actual writes,
         * even when a written pixel had the same color as its background. */
        WORD *alternate=(WORD*)alloc(GPU_CAPTURE_NATIVE_BYTES);
        WORD *inverted=(WORD*)alloc(GPU_CAPTURE_NATIVE_BYTES);
        if(!alternate||!inverted){
            if(alternate)VirtualFree(alternate,0,MEM_RELEASE);
            if(inverted)VirtualFree(inverted,0,MEM_RELEASE);
            VirtualFree(scratch,0,MEM_RELEASE);
            logtext("GPU_COVERAGE ERROR allocation");return MSI_SHADOW_ERROR;
        }
        for(unsigned j=0;j<GPU_CAPTURE_NATIVE_BYTES/2u;j++)
            inverted[j]=(WORD)(((const WORD*)best->before)[j]^65535u);
        MsiGpuCompare ignored;
        int coverage_rc=matrox_vk_shadow_compare(&best->batch,inverted,
            (const WORD*)best->after,alternate,&ignored);
        if(coverage_rc){
            VirtualFree(alternate,0,MEM_RELEASE);VirtualFree(inverted,0,MEM_RELEASE);
            VirtualFree(scratch,0,MEM_RELEASE);
            lognum("GPU_COVERAGE ERROR code=",(unsigned)coverage_rc,NULL,0);return MSI_SHADOW_ERROR;
        }
        MsiGpuCoverage coverage;
        msi_gpu_coverage(&coverage,&best->batch,640u,400u,best->written,
            (const WORD*)best->before,(const WORD*)best->after,scratch,alternate);
        pass=msi_gpu_coverage_pass(&coverage);
        /* Check the whole real-game batch through the single-pass path
         * before any live publication. CPU remains authoritative. */
        if(pass){
            uint32_t *tagged=(uint32_t*)alloc(GPU_CAPTURE_NATIVE_BYTES*2u);
            DWORD tag_start=GetTickCount();
            int tag_rc=tagged?matrox_vk_shadow_tagged(&best->batch,tagged):-1;
            if(tag_rc){
                lognum("GPU_TAGGED_BATCH ERROR code=",(unsigned)tag_rc,NULL,0);
                if(tagged)VirtualFree(tagged,0,MEM_RELEASE);
                VirtualFree(alternate,0,MEM_RELEASE);VirtualFree(inverted,0,MEM_RELEASE);
                VirtualFree(scratch,0,MEM_RELEASE);return MSI_SHADOW_ERROR;
            }
            MsiGpuTaggedCheck check;
            msi_gpu_tagged_check(&check,&best->batch,GPU_CAPTURE_NATIVE_BYTES/2u,
                best->written,scratch,alternate,tagged);
            pass=msi_gpu_tagged_pass(&check,GPU_CAPTURE_NATIVE_BYTES/2u);
            n=0;putstr(msg,&n,pass?"GPU_TAGGED_BATCH PASS frame=":"GPU_TAGGED_BATCH DIFFERENCES frame=");
            putunsigned(msg,&n,g_frames);
            putstr(msg,&n," draws=");putunsigned(msg,&n,best->batch.count);
            putstr(msg,&n," written=");putunsigned(msg,&n,check.written);
            putstr(msg,&n," untouched=");putunsigned(msg,&n,check.untouched);
            putstr(msg,&n," color_diff=");putunsigned(msg,&n,check.color_diff);
            putstr(msg,&n," coverage_diff=");putunsigned(msg,&n,check.coverage_diff);
            putstr(msg,&n," invalid=");putunsigned(msg,&n,check.invalid);
            putstr(msg,&n," homogeneous=");putunsigned(msg,&n,check.homogeneous);
            putstr(msg,&n," provenance_checked=");putunsigned(msg,&n,check.provenance_checked);
            putstr(msg,&n," provenance_diff=");putunsigned(msg,&n,check.provenance_diff);
            putstr(msg,&n," elapsed_ms=");putunsigned(msg,&n,GetTickCount()-tag_start);
            msg[n]=0;logtext(msg);
            if(pass){
                /* Exact full-vs-region comparison on the borrowed device.
                 * No live writes, no bypass change until owner validation. */
                uint32_t *roi=(uint32_t*)alloc(GPU_CAPTURE_NATIVE_BYTES*2u);
                unsigned region_pixels=0,diff=0;
                DWORD roi_start=GetTickCount();
                int roi_rc=roi?matrox_vk_shadow_tagged_roi(&best->batch,roi,&region_pixels):-1;
                if(!roi_rc)for(unsigned j=0;j<GPU_CAPTURE_NATIVE_BYTES/2u;j++)diff+=roi[j]!=tagged[j];
                if(roi)VirtualFree(roi,0,MEM_RELEASE);
                if(roi_rc){
                    lognum("GPU_TAGGED_ROI ERROR code=",(unsigned)roi_rc,NULL,0);
                    VirtualFree(tagged,0,MEM_RELEASE);VirtualFree(alternate,0,MEM_RELEASE);
                    VirtualFree(inverted,0,MEM_RELEASE);VirtualFree(scratch,0,MEM_RELEASE);
                    return MSI_SHADOW_ERROR;
                }
                pass=!diff&&region_pixels>0&&region_pixels<=GPU_CAPTURE_NATIVE_BYTES/2u;
                n=0;putstr(msg,&n,pass?"GPU_TAGGED_ROI PASS frame=":"GPU_TAGGED_ROI DIFFERENCES frame=");
                putunsigned(msg,&n,g_frames);putstr(msg,&n," draws=");putunsigned(msg,&n,best->batch.count);
                putstr(msg,&n," exact=");putunsigned(msg,&n,GPU_CAPTURE_NATIVE_BYTES/2u-diff);
                putstr(msg,&n," bytes=");putunsigned(msg,&n,region_pixels*4u);
                putstr(msg,&n," full_bytes=");putunsigned(msg,&n,GPU_CAPTURE_NATIVE_BYTES*2u);
                putstr(msg,&n," elapsed_ms=");putunsigned(msg,&n,GetTickCount()-roi_start);
                msg[n]=0;logtext(msg);
            }
            if(pass){
                int retirement=gpu_capture_retirement_check(best,tagged,&check,inverted,alternate);
                if(retirement==MSI_SHADOW_ERROR){
                    VirtualFree(tagged,0,MEM_RELEASE);
                    VirtualFree(alternate,0,MEM_RELEASE);VirtualFree(inverted,0,MEM_RELEASE);
                    VirtualFree(scratch,0,MEM_RELEASE);return MSI_SHADOW_ERROR;
                }
                pass=retirement==MSI_SHADOW_PASS;
            }
            VirtualFree(tagged,0,MEM_RELEASE);
        }
        VirtualFree(alternate,0,MEM_RELEASE);VirtualFree(inverted,0,MEM_RELEASE);
        n=0;putstr(msg,&n,msi_gpu_coverage_pass(&coverage)?"GPU_COVERAGE PASS frame=":"GPU_COVERAGE DIFFERENCES frame=");
        putunsigned(msg,&n,g_frames);
        putstr(msg,&n," cpu_written=");putunsigned(msg,&n,coverage.cpu_written);
        putstr(msg,&n," gpu_written=");putunsigned(msg,&n,coverage.gpu_written);
        putstr(msg,&n," same_color=");putunsigned(msg,&n,coverage.same_color);
        putstr(msg,&n," cpu_only=");putunsigned(msg,&n,coverage.cpu_only);
        putstr(msg,&n," gpu_only=");putunsigned(msg,&n,coverage.gpu_only);
        putstr(msg,&n," edge=");putunsigned(msg,&n,coverage.edge);
        putstr(msg,&n," unexplained=");putunsigned(msg,&n,coverage.bad);
        putstr(msg,&n," outside=");putunsigned(msg,&n,coverage.outside);
        putstr(msg,&n," invalid=");putunsigned(msg,&n,coverage.invalid);
        msg[n]=0;logtext(msg);
    }
    if(publish){
        MsiGpuPublish result;
        if(pass&&msi_gpu_publish_verified(&result,(WORD*)g_color,(const WORD*)best->before,
              (const WORD*)best->after,scratch,GPU_CAPTURE_NATIVE_BYTES/2u,&stats)){
            g_gpu_publish.waves++;g_gpu_publish.copied+=result.copied;
            g_gpu_publish.changed+=result.changed;g_gpu_publish.preserved+=result.preserved;
            n=0;putstr(msg,&n,"GPU_PUBLISH OK frame=");putunsigned(msg,&n,g_frames);
            putstr(msg,&n," copied=");putunsigned(msg,&n,result.copied);
            putstr(msg,&n," changed=");putunsigned(msg,&n,result.changed);
            putstr(msg,&n," preserved=");putunsigned(msg,&n,result.preserved);
            msg[n]=0;logtext(msg);
        }else{
            g_gpu_publish.rejected++;
            logtext("GPU_PUBLISH REJECT CPU retained");
        }
    }
    VirtualFree(scratch,0,MEM_RELEASE);
    return pass?1:2;
}
/* Only after a completed CPU triangle, at the first sufficiently large full 32-draw wave of
 * an admitted sparse diagnostic frame. No nested guest callbacks, no F12
 * provenance, no asynchronous guest write, and never before rasterization. */
static int gpu_capture_publish_wave(void){
    if(!g_gpu_capture_sample||!g_vk_active||g_endframe_depth||g_guest_callback_depth||
       g_capture_ownership||g_zplane_draw_context||g_manual_capture_active)return 0;
    if(!msi_gpu_snapshot_equal(g_gpu_capture_after,(const BYTE*)g_color,GPU_CAPTURE_NATIVE_BYTES))return 0;
    const WORD *before=(const WORD*)g_gpu_capture_before,*after=(const WORD*)g_gpu_capture_after;
    unsigned changed=0;
    for(unsigned i=0;i<GPU_CAPTURE_NATIVE_BYTES/2u;i++)changed+=before[i]!=after[i];
    /* Avoid using all 24 samples on tiny first waves. */
    if(changed<16384u)return 0;
    int outcome=gpu_capture_shadow_compare(g_gpu_capture_current,changed,1);
    msi_gpu_shadow_sampler_finish(&g_gpu_shadow_auto,outcome);
    gpu_capture_release();
    return 1;
}
/* A separate immutable replay measures the same admitted triangle on CPU
 * and qualifies the packed Vulkan result before any live publication.
 * Timing excludes scratch allocation and comparison. Not an original
 * rasterizer benchmark: replay deliberately preserves scanline arithmetic. */
static int gpu_bypass_oracle(const MsiGpuDrawBatch *batch,const WORD *before,
    const uint32_t *gpu,unsigned count,MsiGpuRect roi){
    WORD *cpu=(WORD*)alloc(GPU_CAPTURE_NATIVE_BYTES);
    BYTE *mask=(BYTE*)alloc(GPU_CAPTURE_NATIVE_BYTES/2u);
    MSI_LARGE_INTEGER a,b;
    int timed=g_cap_frequency&&QueryPerformanceCounter(&a);
    int replay=cpu&&mask&&msi_gpu_cpu_replay(batch,640u,400u,before,cpu,mask);
    timed=timed&&QueryPerformanceCounter(&b);
    MsiGpuPilotOracle r={0};
    int pass=replay&&msi_gpu_pilot_oracle(&r,batch,cpu,mask,gpu,count,roi);
    DWORD hz=0,ticks=0;
    if(timed&&replay&&b.QuadPart>=a.QuadPart&&b.QuadPart-a.QuadPart<=4294967295LL){
        hz=g_cap_frequency;ticks=(DWORD)(b.QuadPart-a.QuadPart);
    }
    char msg[512];DWORD n=0;
    putstr(msg,&n,pass?"GPU_BYPASS_ORACLE PASS frame=":replay?"GPU_BYPASS_ORACLE DIFFERENCES frame=":"GPU_BYPASS_ORACLE ERROR frame=");
    putunsigned(msg,&n,g_frames);
    putstr(msg,&n," cpu_written=");putunsigned(msg,&n,r.cpu_written);
    putstr(msg,&n," gpu_written=");putunsigned(msg,&n,r.gpu_written);
    putstr(msg,&n," both=");putunsigned(msg,&n,r.both);
    putstr(msg,&n," exact=");putunsigned(msg,&n,r.exact);
    putstr(msg,&n," one_lsb=");putunsigned(msg,&n,r.one);
    putstr(msg,&n," edge_color=");putunsigned(msg,&n,r.edge_color);
    putstr(msg,&n," cpu_only=");putunsigned(msg,&n,r.cpu_only);
    putstr(msg,&n," gpu_only=");putunsigned(msg,&n,r.gpu_only);
    putstr(msg,&n," edge_coverage=");putunsigned(msg,&n,r.edge_coverage);
    putstr(msg,&n," unexplained=");putunsigned(msg,&n,r.bad);
    putstr(msg,&n," outside=");putunsigned(msg,&n,r.outside);
    putstr(msg,&n," invalid=");putunsigned(msg,&n,r.invalid);
    putstr(msg,&n," qpc_hz=");putunsigned(msg,&n,hz);
    putstr(msg,&n," cpu_ticks=");putunsigned(msg,&n,ticks);
    msg[n]=0;logtext(msg);
    if(cpu)VirtualFree(cpu,0,MEM_RELEASE);
    if(mask)VirtualFree(mask,0,MEM_RELEASE);
    return pass;
}
/* Diagnostic-only single-triangle GPU execution, AFTER 24 successful
 * paired color/coverage samples. No batching/deferred guest writes. */
static int gpu_bypass_try(void){
    if(!g_gpu_capture_bypass||!g_gpu_capture_draw||g_gpu_capture_batch.count!=1u||
       !g_vk_active||g_endframe_depth||g_guest_callback_depth||g_capture_ownership||
       g_manual_capture_active||g_zplane_draw_context)return 0;
    const WORD *before=(const WORD*)g_gpu_capture_before;
    const DWORD capture_alloc_ticks=g_gpu_pilot_alloc_ticks,snapshot_ticks=g_gpu_pilot_snapshot_ticks;
    MSI_INT64 scratch_begin=gpu_cost_now();
    MsiGpuRect region={0};
    unsigned region_pixels=0;
    if(msi_gpu_native_tagged_region(&g_gpu_capture_batch,&region))
        region_pixels=(region.x1-region.x0)*(region.y1-region.y0);
    uint32_t *gpu=region_pixels&&region_pixels<=MSI_GPU_PILOT_PACKED_COUNT?g_gpu_pilot_storage.packed:0;
    DWORD scratch_ticks=gpu_cost_elapsed(scratch_begin);
    MsiGpuBypass result;
    DWORD started=GetTickCount();
    g_gpu_bypass.attempted++;
    /* Publish packed ROI tags without expanding a full-frame scratch.
     * Still validate the entire unchanged framebuffer, the exact region,
     * and guard-ring tags before touching any live pixels. */
    MSI_LARGE_INTEGER q0,q1,q2,q3;
    int timed=g_cap_frequency&&QueryPerformanceCounter(&q0);
    uint32_t wait_ticks=UINT32_MAX;
    int rc=gpu?matrox_vk_shadow_tagged_packed(&g_gpu_capture_batch,gpu,region_pixels,&region,&wait_ticks):-1;
    timed=timed&&QueryPerformanceCounter(&q1);
    unsigned texture_mode=rc?3u:matrox_vk_shadow_roi_texture_mode();
    unsigned decoded_bytes=(unsigned)g_gpu_capture_batch.snapshots.decoded_used*4u;
    if(!rc&&(!region_pixels||region_pixels>GPU_CAPTURE_NATIVE_BYTES/2u))rc=-2;
    if(!rc&&!gpu_bypass_oracle(&g_gpu_capture_batch,before,gpu,region_pixels,region))rc=-4;
    timed=timed&&QueryPerformanceCounter(&q2);
    int ok=!rc&&msi_gpu_bypass_tagged_roi_commit(&result,&g_gpu_capture_batch,
        (WORD*)g_color,g_shadow_underlay,before,gpu,region_pixels,region);
    timed=timed&&QueryPerformanceCounter(&q3);
    if(!ok&&!rc)rc=-3;
    if(ok){
        char msg[256];DWORD n=0;
        putstr(msg,&n,"GPU_BYPASS_ROI_PUBLISH PASS frame=");
        putunsigned(msg,&n,g_frames);putstr(msg,&n," draws=1 written=");
        putunsigned(msg,&n,result.written);putstr(msg,&n," bytes=");
        putunsigned(msg,&n,region_pixels*4u);putstr(msg,&n," full_bytes=");
        putunsigned(msg,&n,GPU_CAPTURE_NATIVE_BYTES*2u);
        putstr(msg,&n," elapsed_ms=");putunsigned(msg,&n,GetTickCount()-started);
        msg[n]=0;logtext(msg);
        /* Dispatch is host wall time including allocation/synchronization;
         * publish is the transactional CPU check/copy. Not GPU timestamps. */
        DWORD hz=0,dispatch_ticks=0,publish_ticks=0;
        if(timed&&q1.QuadPart>=q0.QuadPart&&q3.QuadPart>=q2.QuadPart&&
           q1.QuadPart-q0.QuadPart<=4294967295LL&&q3.QuadPart-q2.QuadPart<=4294967295LL){
            hz=g_cap_frequency;dispatch_ticks=(DWORD)(q1.QuadPart-q0.QuadPart);
            publish_ticks=(DWORD)(q3.QuadPart-q2.QuadPart);
        }
        n=0;putstr(msg,&n,"GPU_BYPASS_METRICS frame=");putunsigned(msg,&n,g_frames);
        putstr(msg,&n," mode=");putunsigned(msg,&n,texture_mode);
        putstr(msg,&n," decoded_bytes=");putunsigned(msg,&n,decoded_bytes);
        putstr(msg,&n," upload_bytes=");putunsigned(msg,&n,texture_mode==1u?0u:decoded_bytes);
        putstr(msg,&n," qpc_hz=");putunsigned(msg,&n,hz);
        putstr(msg,&n," dispatch_ticks=");putunsigned(msg,&n,dispatch_ticks);
        putstr(msg,&n," publish_ticks=");putunsigned(msg,&n,publish_ticks);
        unsigned wait_valid=hz&&wait_ticks!=UINT32_MAX&&wait_ticks<=dispatch_ticks;
        putstr(msg,&n," fence_wait_valid=");putunsigned(msg,&n,wait_valid);
        putstr(msg,&n," fence_wait_ticks=");putunsigned(msg,&n,wait_valid?wait_ticks:0);
        msg[n]=0;logtext(msg);
    }
    /* Packed host output is borrowed; the bridge has completed its
     * synchronous readback/copy before returning. Release the entire pool
     * only at pilot retirement/cleanup, measured in capture_free_ticks. */
    DWORD scratch_free_ticks=0;
    if(ok){
        g_gpu_bypass.passed++;g_gpu_bypass.written+=result.written;
        g_gpu_bypass.changed+=result.changed;
        g_drawn_triangles++;g_drawn_pixels+=result.written;
        lognum("GPU_BYPASS OK frame=",g_frames,"written=",result.written);
        lognum("GPU_BYPASS changed=",result.changed,"elapsed_ms=",GetTickCount()-started);
    }else{
        g_gpu_bypass.failed++;g_gpu_bypass.disabled=1;
        lognum("GPU_BYPASS FALLBACK frame=",g_frames,"code=",(unsigned)rc);
    }
    MSI_INT64 release_begin=gpu_cost_now();
    gpu_capture_release();
    DWORD release_ticks=gpu_cost_elapsed(release_begin);
    if(ok){
        unsigned valid=g_cap_frequency&&capture_alloc_ticks!=UINT32_MAX&&snapshot_ticks!=UINT32_MAX&&
            scratch_ticks!=UINT32_MAX&&scratch_free_ticks!=UINT32_MAX&&release_ticks!=UINT32_MAX;
        char msg[300];DWORD n=0;
        putstr(msg,&n,"GPU_BYPASS_COST frame=");putunsigned(msg,&n,g_frames);
        putstr(msg,&n," qpc_hz=");putunsigned(msg,&n,valid?g_cap_frequency:0);
        putstr(msg,&n," capture_alloc_ticks=");putunsigned(msg,&n,valid?capture_alloc_ticks:0);
        putstr(msg,&n," snapshot_ticks=");putunsigned(msg,&n,valid?snapshot_ticks:0);
        putstr(msg,&n," scratch_alloc_ticks=");putunsigned(msg,&n,valid?scratch_ticks:0);
        putstr(msg,&n," scratch_free_ticks=");putunsigned(msg,&n,valid?scratch_free_ticks:0);
        putstr(msg,&n," capture_free_ticks=");putunsigned(msg,&n,valid?release_ticks:0);
        msg[n]=0;logtext(msg);
    }
    return ok;
}
static void gpu_capture_auto_finish(void){
    if(g_gpu_capture_bypass){g_gpu_bypass.empty++;gpu_capture_release();return;}
    if(!g_gpu_capture_sample)return;
    gpu_capture_barrier();
    int outcome=MSI_SHADOW_SKIP;
    if(g_endframe_depth==1u&&!g_guest_callback_depth&&g_vk_active){
        outcome=g_gpu_capture_best_changed?gpu_capture_shadow_compare(g_gpu_capture_best,g_gpu_capture_best_changed,0):MSI_SHADOW_EMPTY;
    }
    msi_gpu_shadow_sampler_finish(&g_gpu_shadow_auto,outcome);
    gpu_capture_release();
}
static void gpu_capture_save(void){
    if(!g_gpu_capture_active)return;
    gpu_capture_barrier();
    if(!g_gpu_capture_best_changed){logtext("GPU_CAPTURE no changed contiguous perspective/bilinear indexed8 wave of at least 8 draws");return;}
    const MsiGpuCaptureSlot *best=g_gpu_capture_best;
    const MsiGpuDrawBatch *batch=&best->batch;
    int shadow_result=gpu_capture_shadow_compare(best,g_gpu_capture_best_changed,0);
    if(shadow_result==MSI_SHADOW_DIFFERENCES||shadow_result==MSI_SHADOW_ERROR)g_gpu_shadow_auto.disabled=1;
    char name[100],path[1024];
    capture_name(name,g_manual_capture_sequence,"_gpu.bin");
    DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(!n||n>=1023)return;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(name[k]&&i+k<1022){path[i+k]=name[k];k++;}
    if(name[k])return;path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){DWORD error=GetLastError();lognum("GPU_CAPTURE create failed win32_error=",error,NULL,0);return;}
    BYTE header[32];const char *magic="M95GPU01";
    for(unsigned j=0;j<8;j++)header[j]=(BYTE)magic[j];
    put32le(header+8,640);put32le(header+12,400);
    put32le(header+16,batch->count);
    put32le(header+20,batch->snapshots.count);
    put32le(header+24,(unsigned)batch->snapshots.decoded_used);
    put32le(header+28,g_frames);
    BOOL ok=gpu_capture_write(h,header,sizeof(header));
    for(unsigned j=0;j<batch->snapshots.count&&ok;j++){
        const MsiGpuSnapshot *s=&batch->snapshots.entries[j];
        BYTE record[12];put32le(record,s->state.width);put32le(record+4,s->state.height);
        put32le(record+8,(unsigned)s->decoded_offset);
        ok=gpu_capture_write(h,record,sizeof(record));
    }
    if(ok)ok=gpu_capture_write(h,batch->draws,batch->count*sizeof(MsiGpuDraw));
    if(ok)ok=gpu_capture_write(h,batch->vertices,batch->count*3u*sizeof(MsiGpuVertex));
    if(ok)ok=gpu_capture_write(h,batch->snapshots.decoded,(DWORD)batch->snapshots.decoded_used*4u);
    if(ok)ok=gpu_capture_write(h,best->before,GPU_CAPTURE_NATIVE_BYTES);
    if(ok)ok=gpu_capture_write(h,best->after,GPU_CAPTURE_NATIVE_BYTES);
    CloseHandle(h);
    char msg[240];DWORD m=0;putstr(msg,&m,ok?"GPU_CAPTURE OK file=":"GPU_CAPTURE WRITE ERROR file=");
    putstr(msg,&m,name);putstr(msg,&m," draws=");putunsigned(msg,&m,batch->count);
    putstr(msg,&m," textures=");putunsigned(msg,&m,batch->snapshots.count);
    putstr(msg,&m," changed=");putunsigned(msg,&m,g_gpu_capture_best_changed);
    putstr(msg,&m," waves_examined=");putunsigned(msg,&m,g_gpu_capture_waves);
    msg[m]=0;logtext(msg);
}
static void capture_frame(const char *filename,unsigned bit){
    /* Only explicit F12 can emit BMP files. Automatic debug calls are inert. */
    if(!g_manual_capture_active)return;
    if((g_snapshot_mask&bit)||!g_initialized||!g_color||
       g_info.msiColor.BytesPerPixel!=2||g_info.msiColor.Pitch<=0)return;
    g_snapshot_mask|=bit;
    const unsigned width=(unsigned)g_info.msiColor.Pitch/2;
    const unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(width==0||height==0||width>4096||height>4096)return;
    const unsigned rowbytes=width*2, stride=(rowbytes+3u)&~3u;
    const unsigned bytes=stride*height;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(n==0||n>=1023)return;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("FRAME_CAPTURE FAILED: file could not be created");return;}
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);put32le(header+10,66u);
    put32le(header+14,40u);put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);put32le(header+62,0x001Fu);
    DWORD written=0;BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    unsigned nonzero=0;const BYTE*base=(const BYTE*)g_color;
    BYTE padding[3]={0,0,0};
    for(unsigned y=0;y<height&&ok;y++){
        const BYTE*row=base+y*(unsigned)g_info.msiColor.Pitch;
        for(unsigned x=0;x<width;x++)if(row[2*x]||row[2*x+1])nonzero++;
        ok=WriteFile(h,row,rowbytes,&written,NULL)&&written==rowbytes;
        if(ok&&stride>rowbytes)ok=WriteFile(h,padding,stride-rowbytes,&written,NULL)&&written==stride-rowbytes;
    }
    CloseHandle(h);
    char buf[940];DWORD pos=0;
    putstr(buf,&pos,ok?"FRAME_CAPTURE OK file=":"FRAME_CAPTURE WRITE ERROR file=");putstr(buf,&pos,filename);
    putstr(buf,&pos," frames=");putunsigned(buf,&pos,g_frames);
    putstr(buf,&pos," param_calls=");putunsigned(buf,&pos,g_params);
    putstr(buf,&pos," nonzero_pixels=");putunsigned(buf,&pos,nonzero);
    putstr(buf,&pos," total_pixels=");putunsigned(buf,&pos,width*height);
    buf[pos]=0;logtext(buf);
}

/* v0.25: 16-bit RGB565 BMPs of the *discarded samples* and the indexed
 * source texture, independent of the final color/ownership buffers. */
static void capture_blue_diagnostic(const char*filename,int atlas){
    if(!g_manual_capture_active)return;
    unsigned width=atlas?g_blue_atlas_width:640u;
    unsigned height=atlas?g_blue_atlas_height:400u;
    if(!g_initialized || (atlas&&!g_blue_atlas_valid) ||
       width==0 || width>640u || height==0 || height>400u)return;
    const unsigned rowbytes=width*2u,stride=(rowbytes+3u)&~3u,bytes=stride*height;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(n==0 || n>=1023)return;
    DWORD i=n;while(i>0 && path[i-1]!='\\' && path[i-1]!='/')i--;
    DWORD k=0;while(filename[k] && i+k<1022){path[i+k]=filename[k];k++;}path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("BLUE_DIAGNOSTIC could not create BMP");return;}
    BYTE hdr[66];zero(hdr,sizeof(hdr));
    hdr[0]='B';hdr[1]='M';put32le(hdr+2,66u+bytes);put32le(hdr+10,66u);
    put32le(hdr+14,40u);put32le(hdr+18,width);put32le(hdr+22,0u-height);
    put16le(hdr+26,1u);put16le(hdr+28,16u);put32le(hdr+30,3u);
    put32le(hdr+34,bytes);put32le(hdr+54,0xF800u);put32le(hdr+58,0x07E0u);put32le(hdr+62,0x001Fu);
    DWORD wr=0;BOOL ok=WriteFile(h,hdr,sizeof(hdr),&wr,NULL)&&wr==sizeof(hdr);
    WORD row[640];const BYTE pad[3]={0,0,0};unsigned hits=0;
    for(unsigned y=0;y<height && ok;y++){
        for(unsigned x=0;x<width;x++){
            WORD color=0;
            if(atlas){
                unsigned j=y*width+x;
                if(g_blue_atlas_planes==8u){
                    unsigned ix=g_blue_atlas_texels[j];
                    color=(WORD)((unsigned)g_blue_atlas_palette[2u*ix]|
                                 ((unsigned)g_blue_atlas_palette[2u*ix+1u]<<8));
                }else if(g_blue_atlas_planes==16u){
                    color=(WORD)((unsigned)g_blue_atlas_texels[2u*j]|
                                 ((unsigned)g_blue_atlas_texels[2u*j+1u]<<8));
                }
                if(color==0x00FFu)hits++;
            }else if(g_blue_mask_frame==g_frames-1u && g_blue_discard_mask[y*640u+x]){
                color=0xF800u;hits++;
            }
            row[x]=color;
        }
        ok=WriteFile(h,row,rowbytes,&wr,NULL)&&wr==rowbytes;
        if(ok&&stride>rowbytes)ok=WriteFile(h,pad,stride-rowbytes,&wr,NULL)&&wr==stride-rowbytes;
    }
    CloseHandle(h);
    char line[400];DWORD pos=0;
    putstr(line,&pos,ok?"BLUE_DIAGNOSTIC OK file=":"BLUE_DIAGNOSTIC WRITE ERROR file=");
    putstr(line,&pos,filename);putstr(line,&pos," width=");putunsigned(line,&pos,width);
    putstr(line,&pos," height=");putunsigned(line,&pos,height);
    putstr(line,&pos," marked_pixels=");putunsigned(line,&pos,hits);
    line[pos]=0;logtext(line);
}

/* Write a *separate* debug bitmap explaining what painted each pixel.
 * Green = 3D textured nonblack, red = 3D textured black,
 * cyan = 3D flat nonblack, magenta = 3D flat black,
 * yellow = Z-plane nonblack, orange = Z-plane black,
 * blue = world color-key transparency, light blue = Z-plane color-key,
 * grey = absent Z-plane atlas texel; black = no MSI raster coverage.
 * Never changes the real game framebuffer. */
static void capture_ownership(const char *filename){
    if(!g_manual_capture_active)return;
    if(!g_initialized || !g_color || !g_ownership || !g_ownership_bytes)return;
    const unsigned width=(unsigned)g_info.msiColor.Pitch/2;
    const unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(width==0 || width>4096 || height==0 || height>4096 || width*height!=g_ownership_bytes)return;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(n==0||n>=1023)return;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("WORLD_COVERAGE WRITE ERROR: open failed");return;}
    unsigned stride=(width*2u+3u)&~3u, bytes=stride*height;
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);put32le(header+10,66u);
    put32le(header+14,40u);put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);put32le(header+62,0x001Fu);
    DWORD written=0;BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    static WORD row[4096]; BYTE pad[3]={0,0,0};
    unsigned counts[10]={0,0,0,0,0,0,0,0,0,0};
    unsigned untouchedBlack=0, untouchedColored=0;
    const WORD*frame=(const WORD*)g_color;
    for(unsigned y=0;y<height&&ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned j=y*width+x, t=g_ownership[j];
            if(t>9)t=0;
            counts[t]++;
            if(!t){if(frame[j])untouchedColored++;else untouchedBlack++;}
            static const WORD colors[10]={0x0000u,0x07E0u,0xF800u,0x07FFu,0xF81Fu,0xFFE0u,0xFC00u,0x001Fu,0x07DFu,0x8410u};
            row[x]=colors[t];
        }
        ok=WriteFile(h,row,width*2,&written,NULL)&&written==width*2;
        if(ok&&stride>width*2)ok=WriteFile(h,pad,stride-width*2,&written,NULL)&&written==stride-width*2;
    }
    CloseHandle(h);
    char buf[420];DWORD z=0;
    putstr(buf,&z,ok?"WORLD_COVERAGE OK file=":"WORLD_COVERAGE WRITE ERROR file=");
    putstr(buf,&z,filename);putstr(buf,&z," frame=");putunsigned(buf,&z,g_frames);
    putstr(buf,&z," world_tex_color=");putunsigned(buf,&z,counts[1]);
    putstr(buf,&z," world_tex_black=");putunsigned(buf,&z,counts[2]);
    putstr(buf,&z," world_flat_color=");putunsigned(buf,&z,counts[3]);
    putstr(buf,&z," world_flat_black=");putunsigned(buf,&z,counts[4]);
    putstr(buf,&z," zplane_color=");putunsigned(buf,&z,counts[5]);
    putstr(buf,&z," zplane_black=");putunsigned(buf,&z,counts[6]);
    putstr(buf,&z," world_key_skip=");putunsigned(buf,&z,counts[7]);
    putstr(buf,&z," hud_key_skip=");putunsigned(buf,&z,counts[8]);
    putstr(buf,&z," hud_missing_texel=");putunsigned(buf,&z,counts[9]);
    putstr(buf,&z," untouched_black=");putunsigned(buf,&z,untouchedBlack);
    putstr(buf,&z," untouched_colored=");putunsigned(buf,&z,untouchedColored);
    buf[z]=0;logtext(buf);
}

/* Exact F12-only sparse-atlas provenance bitmap. This reveals where atlas
 * page-residency guesses and chroma keys were used, NOT which game assets
 * are actually missing. Colors: green=uploaded nonzero, cyan=uploaded zero,
 * magenta=unmarked nonzero, red=unmarked zero skipped, yellow=keyed. */
static void capture_atlas_provenance(const char *filename){
    if(!g_manual_capture_active||!g_initialized||!g_atlas_provenance||
       g_info.msiColor.BytesPerPixel!=2||g_info.msiColor.Pitch<=0)return;
    unsigned width=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!width||width>4096u||!height||height>4096u||
       width*height!=g_ownership_bytes)return;
    char path[1024];DWORD length=GetModuleFileNameA(g_module,path,1023);
    if(!length||length>=1023)return;
    DWORD i=length;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("ATLAS_CAPTURE WRITE ERROR: open failed");return;}
    unsigned stride=(width*2u+3u)&~3u,bytes=stride*height;
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);
    put32le(header+10,66u);put32le(header+14,40u);
    put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);put32le(header+62,0x001Fu);
    DWORD written=0;
    BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    static WORD row[4096];const BYTE pad[3]={0,0,0};
    unsigned counts[6],tiles[4][4][6];
    /* The freestanding MSI95 DLL cannot call the C runtime memset.
     * Volatile writes prevent Clang from replacing fixed-size stack
     * initialization with an unresolved external memset. F12 only. */
    {volatile unsigned *p=(volatile unsigned*)&counts[0];
     for(unsigned i=0;i<6u;i++)p[i]=0u;}
    {volatile unsigned *p=(volatile unsigned*)&tiles[0][0][0];
     for(unsigned i=0;i<4u*4u*6u;i++)p[i]=0u;}
    static const WORD colors[6]={0x0000u,0x07E0u,0x07FFu,0xF81Fu,0xF800u,0xFFE0u};
    for(unsigned y=0;y<height&&ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned kind=g_atlas_provenance[y*width+x];
            if(kind>5u)kind=0;
            row[x]=colors[kind];
            counts[kind]++;
            tiles[y*4u/height][x*4u/width][kind]++;
        }
        ok=WriteFile(h,row,width*2u,&written,NULL)&&written==width*2u;
        if(ok&&stride>width*2u)
            ok=WriteFile(h,pad,stride-width*2u,&written,NULL)&&written==stride-width*2u;
    }
    CloseHandle(h);
    char msg[380];DWORD n=0;
    putstr(msg,&n,ok?"ATLAS_CAPTURE OK file=":"ATLAS_CAPTURE WRITE ERROR file=");
    putstr(msg,&n,filename);
    putstr(msg,&n," frame=");putunsigned(msg,&n,g_frames);
    putstr(msg,&n," uploaded_nonzero=");putunsigned(msg,&n,counts[1]);
    putstr(msg,&n," uploaded_zero=");putunsigned(msg,&n,counts[2]);
    putstr(msg,&n," unmarked_nonzero=");putunsigned(msg,&n,counts[3]);
    putstr(msg,&n," unmarked_zero_skip=");putunsigned(msg,&n,counts[4]);
    putstr(msg,&n," keyed=");putunsigned(msg,&n,counts[5]);
    msg[n]=0;logtext(msg);
    for(unsigned ty=0;ty<4u;ty++)for(unsigned tx=0;tx<4u;tx++){
        if(!tiles[ty][tx][4]&&!tiles[ty][tx][5])continue;
        n=0;
        putstr(msg,&n,"ATLAS_TILE row=");putunsigned(msg,&n,ty);
        putstr(msg,&n," col=");putunsigned(msg,&n,tx);
        putstr(msg,&n," unmarked_zero_skip=");putunsigned(msg,&n,tiles[ty][tx][4]);
        putstr(msg,&n," keyed=");putunsigned(msg,&n,tiles[ty][tx][5]);
        putstr(msg,&n," uploaded_zero=");putunsigned(msg,&n,tiles[ty][tx][2]);
        msg[n]=0;logtext(msg);
    }
}


/* F12-only ordinary world-texture provenance. Categories:
 * 0 none; 1 RGB565 drawn; 2 8bpp nonzero index drawn;
 * 3 8bpp zero index drawn; 4 RGB565 key discarded;
 * 5 indexed key discarded; 6 blue fallback discarded; 7 4bpp drawn.
 * 8=RGB565 keyed raw 8-bit index zero; 9=keyed source index 1;
 * 10=keyed source index 241; 11=keyed any other nonzero index.
 * 12=EXPERIMENTAL opaque source index 1, resolved RGB565 black (amber).
 * 13=EXPERIMENTAL original icon01 source index241 opaque (distinct teal).
 * Last textured world sample only; later untextured or CPU writes can cover it.
 * Never feeds this buffer to the rasterizer or the framebuffer. */
static void capture_world_provenance(const char *filename){
    if(!g_manual_capture_active||!g_initialized||!g_world_provenance||
       g_info.msiColor.BytesPerPixel!=2||g_info.msiColor.Pitch<=0)return;
    unsigned width=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!width||width>4096u||!height||height>4096u||
       width*height!=g_ownership_bytes)return;
    char path[1024];DWORD length=GetModuleFileNameA(g_module,path,1023);
    if(!length||length>=1023)return;
    DWORD i=length;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("WORLD_CAPTURE WRITE ERROR: open failed");return;}
    unsigned stride=(width*2u+3u)&~3u,bytes=stride*height;
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);
    put32le(header+10,66u);put32le(header+14,40u);
    put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);put32le(header+62,0x001Fu);
    DWORD written=0;
    BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    static WORD row[4096];const BYTE pad[3]={0,0,0};
    unsigned counts[14];
    {volatile unsigned *p=(volatile unsigned*)&counts[0];
     for(unsigned i=0;i<14u;i++)p[i]=0u;}
    /* Keyed index 0=magenta; 1=yellow; 241=violet; other=white;
     * previewed opaque index1 is amber. */
    static const WORD colors[14]={0x0000u,0x07E0u,0xAFE5u,0xFD20u,
                                  0x8010u,0xF800u,0x001Fu,0x07FFu,
                                  0xF81Fu,0xFFE0u,0xA01Fu,0xFFFFu,
                                  0xFC00u,0x83E0u};
    for(unsigned y=0;y<height&&ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned kind=g_world_provenance[y*width+x];
            if(kind>13u)kind=0u;
            row[x]=colors[kind];counts[kind]++;
        }
        ok=WriteFile(h,row,width*2u,&written,NULL)&&written==width*2u;
        if(ok&&stride>width*2u)
            ok=WriteFile(h,pad,stride-width*2u,&written,NULL)&&written==stride-width*2u;
    }
    CloseHandle(h);
    char msg[430];DWORD n=0;
    putstr(msg,&n,ok?"WORLD_CAPTURE OK file=":"WORLD_CAPTURE WRITE ERROR file=");
    putstr(msg,&n,filename);
    putstr(msg,&n," frame=");putunsigned(msg,&n,g_frames);
    putstr(msg,&n," rgb16_draw=");putunsigned(msg,&n,counts[1]);
    putstr(msg,&n," indexed_nonzero_draw=");putunsigned(msg,&n,counts[2]);
    putstr(msg,&n," indexed_zero_draw=");putunsigned(msg,&n,counts[3]);
    putstr(msg,&n," rgb565_key=");putunsigned(msg,&n,counts[4]+counts[8]+counts[9]+counts[10]+counts[11]);
    putstr(msg,&n," rgb565_key_index0=");putunsigned(msg,&n,counts[8]);
    putstr(msg,&n," rgb565_key_otherindex=");putunsigned(msg,&n,counts[9]+counts[10]+counts[11]);
    putstr(msg,&n," rgb565_key_unindexed=");putunsigned(msg,&n,counts[4]);
    putstr(msg,&n," index_key=");putunsigned(msg,&n,counts[5]);
    putstr(msg,&n," blue_fallback=");putunsigned(msg,&n,counts[6]);
    putstr(msg,&n," indexed4_draw=");putunsigned(msg,&n,counts[7]);
    msg[n]=0;logtext(msg);
    char sl[180];DWORD sn=0;
    putstr(sl,&sn,"WORLD_KEY_SCREEN frame=");putunsigned(sl,&sn,g_frames);
    putstr(sl,&sn," index1=");putunsigned(sl,&sn,counts[9]);
    putstr(sl,&sn," index241=");putunsigned(sl,&sn,counts[10]);
    putstr(sl,&sn," other_nonzero=");putunsigned(sl,&sn,counts[11]);
    putstr(sl,&sn," index1_opaque_preview=");putunsigned(sl,&sn,counts[12]);
    putstr(sl,&sn," icon01_index241_opaque=");putunsigned(sl,&sn,counts[13]);
    sl[sn]=0;logtext(sl);
    /* The existing key histogram counts only discarded source samples.
     * This independent F12 histogram counts every source-index match
     * BEFORE the optional index-1 preview exception suppresses a skip. */
    {
        unsigned total=0,distinct=0;
        for(unsigned ix=0;ix<256u;ix++){
            total+=g_world_key_candidate_hist[ix];
            if(g_world_key_candidate_hist[ix])distinct++;
        }
        char ab[400];DWORD an=0;
        putstr(ab,&an,"F12_PALETTE_KEY_CANDIDATES frame=");putunsigned(ab,&an,g_frames);
        putstr(ab,&an," attempts=");putunsigned(ab,&an,total);
        putstr(ab,&an," distinct_source_indices=");putunsigned(ab,&an,distinct);
        putstr(ab,&an," source_index0=");putunsigned(ab,&an,g_world_key_candidate_hist[0]);
        putstr(ab,&an," source_index1=");putunsigned(ab,&an,g_world_key_candidate_hist[1]);
        putstr(ab,&an," source_index241=");putunsigned(ab,&an,g_world_key_candidate_hist[241]);
        putstr(ab,&an," index1_preview_attempts=");putunsigned(ab,&an,g_world_index1_preview_attempts);
        ab[an]=0;logtext(ab);
    }
    /* Top eight RGB565-keyed 8-bit source indices, counted at sample time.
     * A single screen pixel can be sampled multiple times by overlapping
     * triangles, so sample counts need not match the colored BMP counts. */
    unsigned total=0,distinct=0;
    for(unsigned i=0;i<256u;i++){
        total+=g_world_rgbkey_index_hist[i];
        if(g_world_rgbkey_index_hist[i])distinct++;
    }
    char histline[256];DWORD hn=0;
    putstr(histline,&hn,"WORLD_KEY_INDEX_HIST frame=");putunsigned(histline,&hn,g_frames);
    putstr(histline,&hn," attempts=");putunsigned(histline,&hn,total);
    putstr(histline,&hn," distinct=");putunsigned(histline,&hn,distinct);
    putstr(histline,&hn," index0=");putunsigned(histline,&hn,g_world_rgbkey_index_hist[0]);
    histline[hn]=0;logtext(histline);
    unsigned listed[8];for(unsigned r=0;r<8u;r++)listed[r]=256u;
    for(unsigned rank=0;rank<8u;rank++){
        unsigned best=256u,bestCount=0;
        for(unsigned i=0;i<256u;i++){
            unsigned count=g_world_rgbkey_index_hist[i];
            if(!count||count<bestCount)continue;
            int already=0;
            for(unsigned r=0;r<rank;r++)if(listed[r]==i){already=1;break;}
            if(already)continue;
            if(count>bestCount||best==256u||i<best){
                best=i;bestCount=count;
            }
        }
        if(best==256u)break;
        listed[rank]=best;
        hn=0;
        putstr(histline,&hn,"WORLD_KEY_INDEX_TOP rank=");putunsigned(histline,&hn,rank+1u);
        putstr(histline,&hn," source_index=");putunsigned(histline,&hn,best);
        putstr(histline,&hn," attempts=");putunsigned(histline,&hn,bestCount);
        histline[hn]=0;logtext(histline);
    }
}

/* Shadow-write origins are F12-only metadata. The last actual MSI write
 * owns the pixel, even when earlier shadow polygons were overdrawn.
 * Unlike shadow histogram attempts, this map can localize visible stains. */
static void capture_shadow_provenance(const char *filename){
    if(!g_manual_capture_active || !g_initialized || !g_shadow_provenance ||
       g_info.msiColor.BytesPerPixel!=2 || g_info.msiColor.Pitch<=0)return;
    unsigned width=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!width || width>4096u || !height || height>4096u ||
       width*height!=g_ownership_bytes)return;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(!n || n>=1023)return;
    DWORD i=n;while(i>0 && path[i-1]!='\\' && path[i-1]!='/')i--;
    DWORD k=0;while(filename[k] && i+k<1022){path[i+k]=filename[k];k++;}
    path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("SHADOW_CAPTURE WRITE ERROR: open failed");return;}
    const unsigned stride=(width*2u+3u)&~3u,bytes=stride*height;
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);
    put32le(header+10,66u);put32le(header+14,40u);
    put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);put32le(header+62,0x001Fu);
    DWORD written=0;
    BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    static WORD row[4096];const BYTE pad[3]={0,0,0};
    unsigned counts[6];{
        volatile unsigned *p=(volatile unsigned*)&counts[0];
        for(unsigned c=0;c<6u;c++)p[c]=0u;
    }
    static const WORD colors[6]={0x0000u,0x07E0u,0xF800u,
                                  0xFFE0u,0x07FFu,0xF81Fu};
    for(unsigned y=0;y<height && ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned kind=g_shadow_provenance[y*width+x];
            if(kind>5u)kind=0u;
            counts[kind]++;
            row[x]=colors[kind];
        }
        ok=WriteFile(h,row,width*2u,&written,NULL)&&written==width*2u;
        if(ok&&stride>width*2u)
            ok=WriteFile(h,pad,stride-width*2u,&written,NULL)&&written==stride-width*2u;
    }
    CloseHandle(h);
    char msg[400];DWORD p=0;
    putstr(msg,&p,ok?"SHADOW_CAPTURE OK file=":"SHADOW_CAPTURE WRITE ERROR file=");
    putstr(msg,&p,filename);
    putstr(msg,&p," frame=");putunsigned(msg,&p,g_frames);
    putstr(msg,&p," blended_with_textured_base=");putunsigned(msg,&p,counts[1]);
    putstr(msg,&p," opaque_no_textured_base=");putunsigned(msg,&p,counts[2]);
    putstr(msg,&p," other_dark_with_base=");putunsigned(msg,&p,counts[3]);
    putstr(msg,&p," other_dark_no_base=");putunsigned(msg,&p,counts[4]);
    putstr(msg,&p," unusual_prior_blend=");putunsigned(msg,&p,counts[5]);
    msg[p]=0;logtext(msg);
}

/* Last successful source texel for each final MSI write, F12 only. Never
 * infer visibility from the preexisting _world.bmp, which tracks attempts. */
static void capture_final_texel_source(const char*filename){
    if(!g_manual_capture_active||!g_initialized||!g_final_texel_source||
       g_info.msiColor.BytesPerPixel!=2||g_info.msiColor.Pitch<=0)return;
    unsigned width=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!width||width>4096u||!height||height>4096u||
       width*height!=g_ownership_bytes)return;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(!n||n>=1023)return;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}
    if(filename[k])return;
    path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("FINAL_TEXEL_CAPTURE WRITE ERROR: open failed");return;}
    const unsigned stride=(width*2u+3u)&~3u,bytes=stride*height;
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);put32le(header+10,66u);
    put32le(header+14,40u);put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);put32le(header+62,0x001Fu);
    static const WORD colors[12]={0x0000u,0xFFE0u,0xF81Fu,0x07FFu,
        0xF800u,0x07E0u,0x8010u,0xAFE5u,0xFC00u,0x83E0u,0xFD20u,0x001Fu};
    unsigned counts[12];{volatile unsigned*p=(volatile unsigned*)&counts[0];
        for(unsigned c=0;c<12u;c++)p[c]=0u;}
    DWORD written=0;
    BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    static WORD row[4096];const BYTE pad[3]={0,0,0};
    for(unsigned y=0;y<height&&ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned kind=g_final_texel_source[y*width+x];
            if(kind>=12u)kind=0u;
            row[x]=colors[kind];counts[kind]++;
        }
        ok=WriteFile(h,row,width*2u,&written,NULL)&&written==width*2u;
        if(ok&&stride>width*2u)ok=WriteFile(h,pad,stride-width*2u,&written,NULL)&&written==stride-width*2u;
    }
    CloseHandle(h);
    char msg[380];DWORD q=0;
    putstr(msg,&q,ok?"FINAL_TEXEL_CAPTURE OK file=":"FINAL_TEXEL_CAPTURE WRITE ERROR file=");
    putstr(msg,&q,filename);putstr(msg,&q," frame=");putunsigned(msg,&q,g_frames);
    putstr(msg,&q," index1_black=");putunsigned(msg,&q,counts[1]);
    putstr(msg,&q," index241_black=");putunsigned(msg,&q,counts[2]);
    putstr(msg,&q," index0_black=");putunsigned(msg,&q,counts[3]);
    putstr(msg,&q," other_index_black=");putunsigned(msg,&q,counts[4]);
    putstr(msg,&q," rgb16_black=");putunsigned(msg,&q,counts[6]);
    putstr(msg,&q," rgb4_black=");putunsigned(msg,&q,counts[8]);
    msg[q]=0;logtext(msg);
}

/* Packed F12 raw source-index + pre-light/lighting flags, not a picture.
 * _sourceaudit.bmp is BI_BITFIELDS RGB565 only as a lossless 16-bit carrier.
 * The source map is cleared on F12 and tracks FINAL successful MSI writes. */
static void capture_final_source_audit(const char*filename){
    if(!g_manual_capture_active||!g_initialized||!g_final_source_audit||
       g_info.msiColor.BytesPerPixel!=2||g_info.msiColor.Pitch<=0)return;
    unsigned width=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!width||width>4096u||!height||height>4096u||
       width*height!=g_ownership_bytes)return;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(!n||n>=1023)return;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}
    if(filename[k])return;
    path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("SOURCE_AUDIT_CAPTURE WRITE ERROR: open failed");return;}
    const unsigned stride=(width*2u+3u)&~3u,bytes=stride*height;
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);
    put32le(header+10,66u);put32le(header+14,40u);
    put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);
    put32le(header+62,0x001Fu);
    DWORD written=0;
    BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    static WORD row[4096];const BYTE pad[3]={0,0,0};
    unsigned textured=0u,black_raw=0u,lit=0u;
    for(unsigned y=0;y<height && ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned encoded=g_final_source_audit[y*width+x];
            row[x]=(WORD)encoded;
            if(encoded&0x7000u){
                textured++;
                if(encoded&0x0100u)black_raw++;
                if(encoded&0x0200u)lit++;
            }
        }
        ok=WriteFile(h,row,width*2u,&written,NULL)&&written==width*2u;
        if(ok && stride>width*2u)
            ok=WriteFile(h,pad,stride-width*2u,&written,NULL)&&written==stride-width*2u;
    }
    CloseHandle(h);
    char message[350];DWORD p=0;
    putstr(message,&p,ok?"SOURCE_AUDIT_CAPTURE OK file=":
                "SOURCE_AUDIT_CAPTURE WRITE ERROR file=");
    putstr(message,&p,filename);
    putstr(message,&p," frame=");putunsigned(message,&p,g_frames);
    putstr(message,&p," final_textured=");putunsigned(message,&p,textured);
    putstr(message,&p," prelight_black=");putunsigned(message,&p,black_raw);
    putstr(message,&p," modulated=");putunsigned(message,&p,lit);
    message[p]=0;logtext(message);
}

/* F12-only counterfactual image: no dither for the LAST successful MSI
 * world-texture write, same other settings and prior framebuffer underlay.
 * IMPORTANT this is NOT a full-frame no-dither replay, since earlier draws
 * and their underlays may have been dithered. Only sourceaudit bit10 pixels
 * have a valid reference; all other pixels encode zero. */
static void capture_undithered_reference(const char*filename){
    if(!g_manual_capture_active||!g_initialized||!g_undithered_reference||
       !g_final_source_audit||g_info.msiColor.BytesPerPixel!=2||
       g_info.msiColor.Pitch<=0)return;
    unsigned width=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!width||width>4096u||!height||height>4096u||
       width*height!=g_ownership_bytes)return;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(!n||n>=1023)return;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}
    if(filename[k])return;
    path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){logtext("UNDITHERED_CAPTURE WRITE ERROR: open failed");return;}
    unsigned stride=(width*2u+3u)&~3u,bytes=stride*height;
    BYTE header[66];zero(header,sizeof(header));
    header[0]='B';header[1]='M';put32le(header+2,66u+bytes);
    put32le(header+10,66u);put32le(header+14,40u);
    put32le(header+18,width);put32le(header+22,0u-height);
    put16le(header+26,1u);put16le(header+28,16u);
    put32le(header+30,3u);put32le(header+34,bytes);
    put32le(header+54,0xF800u);put32le(header+58,0x07E0u);
    put32le(header+62,0x001Fu);
    DWORD written=0;
    BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    static WORD row[4096];const BYTE pad[3]={0,0,0};
    unsigned tracked=0u,changed=0u,actual_black_ref_nonblack=0u;
    unsigned actual_black_ref_black=0u,actual_nonblack_ref_black=0u;
    const WORD *displayed=(const WORD*)g_color;
    for(unsigned y=0;y<height&&ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned pos=y*width+x;
            unsigned tracked_here=(g_final_source_audit[pos]&0x0400u)!=0u;
            unsigned raw=(unsigned)g_undithered_reference[pos];
            row[x]=(WORD)(tracked_here?raw:0u);
            if(tracked_here){
                unsigned actual=(unsigned)displayed[pos];
                tracked++;
                if(actual!=raw)changed++;
                if(actual==0u){
                    if(raw)actual_black_ref_nonblack++;
                    else actual_black_ref_black++;
                }else if(raw==0u)actual_nonblack_ref_black++;
            }
        }
        ok=WriteFile(h,row,width*2u,&written,NULL)&&written==width*2u;
        if(ok&&stride>width*2u)
            ok=WriteFile(h,pad,stride-width*2u,&written,NULL)&&written==stride-width*2u;
    }
    CloseHandle(h);
    char msg[490];DWORD q=0;
    putstr(msg,&q,ok?"UNDITHERED_CAPTURE OK file=":"UNDITHERED_CAPTURE WRITE ERROR file=");
    putstr(msg,&q,filename);putstr(msg,&q," frame=");putunsigned(msg,&q,g_frames);
    putstr(msg,&q," tracked=");putunsigned(msg,&q,tracked);
    putstr(msg,&q," changed_by_dither=");putunsigned(msg,&q,changed);
    putstr(msg,&q," actual_black_ref_nonblack=");putunsigned(msg,&q,actual_black_ref_nonblack);
    putstr(msg,&q," actual_black_ref_black=");putunsigned(msg,&q,actual_black_ref_black);
    putstr(msg,&q," actual_nonblack_ref_black=");putunsigned(msg,&q,actual_nonblack_ref_black);
    msg[q]=0;logtext(msg);
}

/* The ninth F12 diagnostic is a versioned BINARY data map, not a bitmap.
 * Header (24 bytes): ASCII "M95LIGHT" + little-endian u32 width,height,
 * record_bytes=5, lighting_model=0 (legacy) or 1 (experimental RGB888). Each pixel is [RGB565 sample LE16,
 * modifier_r8,modifier_g8,modifier_b8]. sourceaudit marks validity.
 * It never alters renderer pixels, keys, fog, shading or texture content. */
static void capture_final_light_inputs(const char*filename){
    if(!g_manual_capture_active||!g_initialized||!g_final_light_inputs||
       !g_final_source_audit||g_info.msiColor.BytesPerPixel!=2||
       g_info.msiColor.Pitch<=0)return;
    unsigned width=(unsigned)g_info.msiColor.Pitch/2u;
    unsigned height=(unsigned)g_info.msiColor.MemSize/(unsigned)g_info.msiColor.Pitch;
    if(!width||width>4096u||!height||height>4096u||
       width*height!=g_ownership_bytes)return;
    char path[1024];DWORD n=GetModuleFileNameA(g_module,path,1023);
    if(!n||n>=1023)return;
    DWORD i=n;while(i>0&&path[i-1]!='\\'&&path[i-1]!='/')i--;
    DWORD k=0;while(filename[k]&&i+k<1022){path[i+k]=filename[k];k++;}
    if(filename[k])return;
    path[i+k]=0;
    HANDLE h=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE){
        logtext("LIGHTING_INPUT_CAPTURE WRITE ERROR: open failed");return;
    }
    BYTE header[24];zero(header,sizeof(header));
    header[0]='M';header[1]='9';header[2]='5';header[3]='L';
    header[4]='I';header[5]='G';header[6]='H';header[7]='T';
    put32le(header+8u,width);put32le(header+12u,height);
    put32le(header+16u,5u);
    put32le(header+20u,(unsigned)g_rgb888_lighting_enabled);
    const unsigned bytes=g_ownership_bytes*5u;
    DWORD written=0;
    BOOL ok=WriteFile(h,header,sizeof(header),&written,NULL)&&written==sizeof(header);
    if(ok)ok=WriteFile(h,g_final_light_inputs,bytes,&written,NULL)&&written==bytes;
    CloseHandle(h);
    unsigned tracked=0u,modulated=0u,black_final=0u;
    const WORD*displayed=(const WORD*)g_color;
    for(unsigned pos=0;pos<g_ownership_bytes;pos++){
        unsigned meta=g_final_source_audit[pos];
        if(!(meta&0x7000u))continue;
        tracked++;
        if(meta&0x0200u)modulated++;
        if(displayed[pos]==0u)black_final++;
    }
    char msg[360];DWORD p=0;
    putstr(msg,&p,ok?"LIGHTING_INPUT_CAPTURE OK file=":
        "LIGHTING_INPUT_CAPTURE WRITE ERROR file=");
    putstr(msg,&p,filename);
    putstr(msg,&p," frame=");putunsigned(msg,&p,g_frames);
    putstr(msg,&p," tracked=");putunsigned(msg,&p,tracked);
    putstr(msg,&p," modulated=");putunsigned(msg,&p,modulated);
    putstr(msg,&p," final_black=");putunsigned(msg,&p,black_final);
    msg[p]=0;logtext(msg);
}

/* Exported functions: MSI95 SDK explicitly declares undecorated extern "C". */
T_msiInfo* __cdecl msiInit(LONG Width,LONG Height,LONG Planes,BOOL ZBuffer,BOOL Debug,WNDPROC proc){
    ensure_log();log_runtime_host();loginit(Width,Height,Planes,ZBuffer,Debug);
    logtext("MATROX_INDEX1_OPAQUE_PREVIEW enabled=1 scope=world_8bpp_128x128_black_rgb565_key; ONLY experimental stacked PR");
    g_guest_window_proc = (WINWNDPROC)proc;
    { char buf[160]; DWORD n=0;
      putstr(buf,&n,"GAME_WNDPROC supplied=");puthex(buf,&n,(unsigned)proc);
      putstr(buf,&n,proc?" enabled":" NULL: input callback unavailable");
      buf[n]=0;logtext(buf);
    }
    if(g_initialized)clean();
    g_boot_tick=GetTickCount();
    g_raster_ms=g_preview_ms=g_state_ms=0;
    g_last_triangle_tick=g_boot_tick;
    g_last_triangle_frame=0;g_no_geometry_logged=0;
    g_shutdown_started=0;
    g_last_left_click_ms=0;
    g_last_heap_alloc_tick=0;g_transition_space_char_ignored=0;
    g_transition_space_char_evaluations=0;
    g_game_50dcd0_last=g_game_50dcc0_last=0xFFFFFFFFu;
    g_game_50dcd0_reports=0;
    g_early_mouse_ignored=g_early_mouse_ready_logs=0;
    g_swallowed_left_down=g_swallowed_right_down=0;
    g_zplane_draw_context=0;
    g_zplane_blue_key_skips=0;
    g_sparse_atlas_triangles=g_sparse_atlas_drawn_pixels=0;
    g_sparse_atlas_unmapped_pixels=g_sparse_atlas_direct_pixels=0;
    g_sparse_atlas_log_count=0;
    zero(g_triangle_opacity,sizeof(g_triangle_opacity));
    zero(g_zplane_opacity,sizeof(g_zplane_opacity));
    g_capture_ownership=0;g_world_texture_samples=0;
    g_fx_param_updates=0;
    g_fx_dither_parameters=g_fx_modulate_parameters=g_fx_decal_parameters=0;
    g_fx_dither_textured_states=g_fx_modulate_textured_states=0;
    g_fx_dither_triangles=g_fx_modulate_triangles=g_fx_decal_triangles=0;
    g_fx_dither_zplanes=g_fx_modulate_zplanes=g_fx_decal_zplanes=0;
    g_fx_vertex_color_logs=g_fx_param_state_logs=0;
    g_modulate_triangles=g_modulate_pixels=g_modulate_varying=0;
    g_dither_candidates=g_dither_changed=g_dither_bypassed=0;
    logtext("MSI_FX_AUDIT v0.35 optional RGB565 dithering; early mouse guard and fixed 60 FPS cap");

    g_perspective_triangles=g_perspective_pixels=0;
    g_world_varying_invw=g_world_constant_invw=g_world_invalid_invw=0;
    g_world_bilinear_triangles=g_world_bilinear_samples=g_world_bilinear_changed=0;
    g_world_bilinear_keyed_bypass=0;
    g_world_bilinear_palette_triangles=0;
    g_world_bilinear_palette_samples=0;
    g_world_bilinear_uniform_samples=0;
    g_world_blue_samples=g_world_blue_eligible=g_world_blue_skipped=0;
    g_world_blue_diagnostic_count=0;
    g_index_key_candidates=g_index_key_skipped=0;
    g_index_key_blue_candidates=g_index_key_other_candidates=g_index_key_debug=0;
    g_shadow_dark_candidates=g_shadow_dark_blended=g_shadow_diagnostic_count=0;
    g_shadow_underlay_pixels=g_shadow_underlay_opaque=0;
    g_shadow_underlay_transparent=g_shadow_without_underlay=0;
    g_shadow_underlay_blended=g_shadow_underlay_debug=0;
    g_shadow_once_blended=g_shadow_once_repeated=0;
    g_shadow_once_no_base=g_shadow_once_nonshadow_resets=0;
    g_manual_capture_pending=g_manual_capture_active=0;
    g_manual_capture_sequence=g_f12_held=0;
    g_auto_blue_frame_samples=g_auto_blue_first_captured=0;
    g_auto_blue_pair_pending=g_auto_blue_pair_captured=0;
    g_blue_mask_frame=0xFFFFFFFFu;g_blue_atlas_valid=0;
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_SHADOW_BLEND",setting,sizeof(setting));
      g_shadow_blend_enabled=(n==1u && setting[0]=='1');}
    logtext(g_shadow_blend_enabled?"SHADOW_BLEND enabled (opt-in flat dark world 50pct alpha)":"SHADOW_BLEND disabled (reference dark world pixels)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_SHADOW_UNDERLAY",setting,sizeof(setting));
      g_shadow_underlay_enabled=(n==1u && setting[0]=='1');}
    if(g_shadow_underlay_enabled && g_shadow_blend_enabled){
        logtext("SHADOW_UNDERLAY cannot combine with SHADOW_BLEND; disabled");
        g_shadow_underlay_enabled=0;
    }
    logtext(g_shadow_underlay_enabled?
        "SHADOW_UNDERLAY enabled (experimental: blend dark flat pixels only on same-frame textured world underlay)":
        "SHADOW_UNDERLAY disabled (reference shadow behavior)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_SHADOW_ONCE",setting,sizeof(setting));
      g_shadow_once_enabled=(n==1u && setting[0]=='1');}
    if(g_shadow_once_enabled && (g_shadow_underlay_enabled || g_shadow_blend_enabled)){
        logtext("SHADOW_ONCE cannot combine with SHADOW_BLEND or SHADOW_UNDERLAY; disabled");
        g_shadow_once_enabled=0;
    }
    logtext(g_shadow_once_enabled?
        "SHADOW_ONCE enabled (experimental 50pct dark-flat, once per pixel on same-frame textured world)":
        "SHADOW_ONCE disabled (reference shadow behavior)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_WORLD_BLUE_KEY",setting,sizeof(setting));
      g_world_blue_key_enabled=(n==1u && setting[0]=='1');}
    logtext(g_world_blue_key_enabled?"WORLD_BLUE_KEY enabled (transparent world triangles only)":"WORLD_BLUE_KEY disabled (reference world textures)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_INDEXED_KEY",setting,sizeof(setting));
      g_index_key_enabled=(n==1u && setting[0]=='1');}
    logtext(g_index_key_enabled?"INDEXED_KEY enabled (world 8bit transparent textures only)":"INDEXED_KEY disabled (reference RGB565 color-key)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_ICON01_OPAQUE",setting,sizeof(setting));
      g_icon01_opaque_enabled=(n==1u && setting[0]=='1');}
    g_icon01_cache_offset=0xffffffffu;
    g_icon01_verified_uploads=g_icon01_opaque_samples=0;
    logtext(g_icon01_opaque_enabled?
        "ICON01_OPAQUE enabled (EXPERIMENTAL: original 128x128 icon01 index241 black, texture fingerprint verified)":
        "ICON01_OPAQUE disabled (reference RGB565 keyed icon01 background)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_PERSPECTIVE",setting,sizeof(setting));
      g_perspective_enabled=(n==1u && setting[0]=='1');}
    logtext(g_perspective_enabled?"PERSPECTIVE_MODE enabled (world triangles only)":"PERSPECTIVE_MODE disabled (v0.19 reference rasterization)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_WORLD_BILINEAR",setting,sizeof(setting));
      g_world_bilinear_enabled=(n==1u && setting[0]=='1');}
    logtext(g_world_bilinear_enabled?
        "WORLD_BILINEAR enabled (EXPERIMENTAL modern 3D opaque world filter, UI/HUD bypassed)":
        "WORLD_BILINEAR disabled (original Mystique nearest texture sampling)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_MODULATE",setting,sizeof(setting));
      g_msi_modulate_enabled=(n==1u && setting[0]=='1');}
    logtext(g_msi_modulate_enabled?"MSI_MODULATE enabled (opt-in per-vertex RGB, textured triangles only; Z-plane HUD excluded)":
        "MSI_MODULATE disabled (v0.32 reference textured colors)");
    {char setting[8];DWORD n=GetEnvironmentVariableA("MATROX_MSI_DITHER",setting,sizeof(setting));
      g_msi_dither_enabled=(n==1u && setting[0]=='1');}
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_RGB888_LIGHTING",setting,sizeof(setting));
      g_rgb888_lighting_enabled=(n==1u && setting[0]=='1');}
    logtext(g_rgb888_lighting_enabled?"MSI_LIGHTING_MODEL RGB888_DIV256_EXPERIMENTAL":
            "MSI_LIGHTING_MODEL LEGACY_RGB565_DIV255");
    logtext(g_msi_dither_enabled?(g_rgb888_lighting_enabled?
            "MSI_DITHER enabled (experimental 2x2 RGB888 quantization; guest Dither gate unchanged)":
            "MSI_DITHER enabled (experimental 4x4 ordered RGB565, modulated 3D world only)"):
            "MSI_DITHER disabled (no ordered quantization)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_PRESENTATION_SMOOTH",setting,sizeof(setting));
      g_presentation_smooth_enabled=(n==1u && setting[0]=='1');}
    g_presentation_smooth_failed=g_presentation_smooth_reported=0;
    g_vk_requested=g_vk_attempted=g_vk_active=0;g_vk_frames=g_vk_skipped=g_vk_ms=g_vk_recreates=g_vk_retry_streak=0;
    {char setting[16]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_RENDERER",setting,sizeof(setting));
      g_vk_requested=(n==4u&&setting[0]=='a'&&setting[1]=='u'&&setting[2]=='t'&&setting[3]=='o')||
        (n==6u&&setting[0]=='v'&&setting[1]=='u'&&setting[2]=='l'&&setting[3]=='k'&&setting[4]=='a'&&setting[5]=='n');
    }
    zero(&g_gpu_profile,sizeof(g_gpu_profile));msi_gpu_wave_reset(&g_gpu_profile_wave);
    zero(&g_gpu_defer_audit,sizeof(g_gpu_defer_audit));zero(&g_gpu_defer_strict,sizeof(g_gpu_defer_strict));
    g_gpu_defer_strict_seen=g_gpu_defer_strict_state=g_gpu_defer_strict_early=0;
    g_gpu_defer_strict_numerical=g_gpu_defer_strict_alias=0;
    g_gpu_profile_active=0;g_gpu_profile_enabled=g_vk_requested&&g_log!=NULL;
    zero(&g_gpu_shadow_auto,sizeof(g_gpu_shadow_auto));
    zero(&g_gpu_publish,sizeof(g_gpu_publish));zero(&g_gpu_bypass,sizeof(g_gpu_bypass));
    g_gpu_pilot_allocations=g_gpu_pilot_reuses=g_gpu_pilot_releases=0;
    if(g_gpu_profile_enabled){
        logtext("GPU_SHADOW_AUTO enabled sample=1/120 max_passes=24 file_output=0 CPU_reference=1 guarded_publish=1");
        logtext("GPU_BYPASS pilot after=24_passes max_triangles=12 sample=1/120 native_only=1 tagged_passes=1");
    }
    if(g_gpu_profile_enabled)logtext("GPU_PROFILE enabled sample=1/60 target=1440x1080 hypothetical_only=1 scope=perspective_or_bilinear_indexed8_opaque");
    logtext(g_vk_requested?"PREVIEW_RENDERER requested=vulkan (experimental; GDI fallback)":"PREVIEW_RENDERER requested=software");
    logtext(g_vk_requested?(g_presentation_smooth_enabled?"PREVIEW_SCALER requested=GPU linear (GDI HALFTONE fallback)":"PREVIEW_SCALER requested=GPU nearest (GDI legacy fallback)"):
        (g_presentation_smooth_enabled?"PREVIEW_SCALER requested=HALFTONE":"PREVIEW_SCALER requested=legacy"));
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_BORDERLESS",setting,sizeof(setting));
      g_borderless_enabled=(n==1u && setting[0]=='1');}
    logtext(g_borderless_enabled?"PREVIEW_BORDERLESS requested: desktop-sized WS_POPUP, centered source 8:5":"PREVIEW_BORDERLESS disabled: original 960x600 window");
    if(Width<1||Width>4096||Height<1||Height>4096||(Planes!=8&&Planes!=16&&Planes!=32)){
       logtext("msiInit UNSUPPORTED dimensions or pixel format");return NULL;
    }
    unsigned bytesPerPixel=(unsigned)Planes/8;
    unsigned pitch=(unsigned)Width*bytesPerPixel;
    unsigned framesize=pitch*(unsigned)Height;
    unsigned zsize=(unsigned)Width*(unsigned)Height*2u;
    unsigned texsize=16u*1024u*1024u;
    g_gpu_dual_checks=g_gpu_dual_mismatch=g_gpu_dual_thread_checks=g_gpu_dual_foreign=0;
    g_gpu_dual_owner=g_gpu_profile_enabled?GetCurrentThreadId():0u;
    color_allocate(framesize,g_gpu_profile_enabled&&Width==640&&Height==400&&Planes==16);
    g_depth=alloc(zsize);g_texture=alloc(texsize);
    g_ownership_bytes=(unsigned)Width*(unsigned)Height;
    g_ownership=(BYTE*)alloc(g_ownership_bytes);
    g_atlas_provenance=(BYTE*)alloc(g_ownership_bytes);
    g_world_provenance=(BYTE*)alloc(g_ownership_bytes);
    g_shadow_provenance=(BYTE*)alloc(g_ownership_bytes);
    g_final_texel_source=(BYTE*)alloc(g_ownership_bytes);
    g_final_source_audit=(WORD*)alloc(g_ownership_bytes*2u);
    g_undithered_reference=(WORD*)alloc(g_ownership_bytes*2u);
    g_final_light_inputs=(BYTE*)alloc(g_ownership_bytes*5u);
    if(!g_final_light_inputs)
        logtext("LIGHTING_INPUT_CAPTURE disabled: optional diagnostic allocation failed");
    if(!g_undithered_reference)
        logtext("UNDITHERED_CAPTURE disabled: optional reference allocation failed");
    if(!g_final_source_audit)
        logtext("SOURCE_AUDIT_CAPTURE disabled: optional metadata allocation failed");
    if(!g_final_texel_source)logtext("FINAL_TEXEL_CAPTURE disabled: diagnostic allocation failed");
    if(!g_shadow_provenance)logtext("SHADOW_CAPTURE disabled: optional diagnostic allocation failed");
    if(!g_world_provenance)logtext("WORLD_CAPTURE disabled: optional diagnostic allocation failed");
    if(!g_atlas_provenance)logtext("ATLAS_CAPTURE disabled: optional diagnostic allocation failed");
    g_shadow_underlay=(g_shadow_underlay_enabled||g_shadow_once_enabled)?(BYTE*)alloc(g_ownership_bytes):NULL;
    if(!g_color||!g_depth||!g_texture||!g_ownership||
       ((g_shadow_underlay_enabled||g_shadow_once_enabled) && !g_shadow_underlay)){
        logtext("msiInit OUT OF MEMORY");clean();return NULL;
    }
    zero(&g_info,sizeof(g_info));
    g_info.VersionMajor=1;g_info.VersionMinor=10;
    g_info.msiColor.BytesPerPixel=(LONG)bytesPerPixel;
    if(Planes==16){g_info.msiColor.rDepth=5;g_info.msiColor.gDepth=6;g_info.msiColor.bDepth=5;
      g_info.msiColor.rBitShift=11;g_info.msiColor.gBitShift=5;g_info.msiColor.bBitShift=0;}
    else if(Planes==32){g_info.msiColor.rDepth=8;g_info.msiColor.gDepth=8;g_info.msiColor.bDepth=8;g_info.msiColor.aDepth=8;
      g_info.msiColor.rBitShift=16;g_info.msiColor.gBitShift=8;g_info.msiColor.aBitShift=24;}
    else {g_info.msiColor.rDepth=3;g_info.msiColor.gDepth=3;g_info.msiColor.bDepth=2;
      g_info.msiColor.rBitShift=5;g_info.msiColor.gBitShift=2;}
    g_info.msiColor.Pitch=(LONG)pitch;g_info.msiColor.pMem=(LPBYTE)g_color_guest;g_info.msiColor.MemSize=(LONG)framesize;
    g_info.msiDepth.BytesPerPixel=2;g_info.msiDepth.zDepth=16;g_info.msiDepth.Pitch=Width*2;
    g_info.msiDepth.pMem=(LPBYTE)g_depth;g_info.msiDepth.MemSize=(LONG)zsize;
    g_info.msiTexture.pMem=(LPBYTE)g_texture;g_info.msiTexture.MemSize=(LONG)texsize;
    g_initialized=1;
    g_snapshot_mask=0;
    logtext("AUTO_BMP_CAPTURE disabled: F12 manual screenshots only");
    g_state_valid=0;
    zero(g_texture_uploaded,sizeof(g_texture_uploaded));
    g_tex_uploads=g_palette_uploads=g_drawn_triangles=g_drawn_pixels=0;
    g_disabled_tex_uploads=g_disabled_lut_uploads=g_direct_cache_reads=0;
    g_missing_textures=g_rejected_triangles=0;
    g_vertex_region_reuses=g_vertex_region_fallbacks=0;
    g_zplanes=0;g_drawn_zplanes=g_drawn_zpixels=g_rejected_zplanes=0;
    g_first_zplane_frame=0xFFFFFFFFu;
    g_cap_qpc_ready=0;
    g_cap_target_fps=60u;
    {char setting[16]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_FPS_LIMIT",setting,sizeof(setting));
     if(n>0 && n<sizeof(setting)){
         unsigned fps=0u,valid=1u;
         for(DWORD i=0;i<n;i++){
             if(setting[i]<'0'||setting[i]>'9'){valid=0u;break;}
             fps=fps*10u+(unsigned)(setting[i]-'0');
             if(fps>240u){valid=0u;break;}
         }
         if(valid)g_cap_target_fps=fps;
     }
    }
    g_cap_frequency=g_cap_last_tick=0;
    g_cap_last_ms=0;
    g_cap_waited_frames=g_cap_late_frames=g_cap_sleep_ms=g_cap_fallback_frames=0;
    {MSI_LARGE_INTEGER freq;
     if(QueryPerformanceFrequency(&freq) && freq.QuadPart>=100000LL && freq.QuadPart<=1000000000LL){
         g_cap_frequency=(DWORD)freq.QuadPart;
         g_cap_qpc_ready=1;
     }
    }
    {char buf[180];DWORD n=0;
      putstr(buf,&n,"FPS_CAP configured maximum=");putunsigned(buf,&n,g_cap_target_fps);
      putstr(buf,&n,g_cap_target_fps?" FPS at MSI95 end-of-frame (QPC with fallback)":" (frame pacing disabled)");
      buf[n]=0;logtext(buf);
    }
    logtext("msiInit OK: allocated RAM framebuffer, Z buffer, texture cache");
    preview_open();
    return &g_info;
}
BOOL __cdecl msiExit(void){
    /* The OEM game may take extra time to tear down after calling MSI exit.
     * Hide the preview immediately, but leave all actual freeing synchronous.
     * This must not synthesize a game Quit event or terminate its process. */
    DWORD exit_tick=GetTickCount();
    g_shutdown_started=1;
    if(g_last_left_click_ms)
        lognum("SHUTDOWN_TIMING last_click_to_msiExit_ms=", exit_tick-g_last_left_click_ms,NULL,0);
    else
        logtext("SHUTDOWN_TIMING no_left_click_recorded");
    if(g_preview){
        ShowWindow(g_preview,0); /* SW_HIDE -- keep normal WM_DESTROY in clean() */
        lognum("SHUTDOWN_PREVIEW_HIDDEN elapsed_ms=",GetTickCount()-exit_tick,NULL,0);
    }else{
        logtext("SHUTDOWN_PREVIEW already absent");
    }
    ensure_log();lognum("msiExit total_frames=",g_frames,"triangles=",g_triangles);
    lognum("msiExit total_zplanes=",g_zplanes,"parameter_updates=",g_params);
    log_opacity_histogram("exit");
    gpu_profile_report();
    if(g_gpu_profile_enabled){
        msi_gpu_shadow_sampler_finish(&g_gpu_shadow_auto,MSI_SHADOW_SKIP);
        lognum("GPU_SHADOW_AUTO sampled=",g_gpu_shadow_auto.sampled,"empty=",g_gpu_shadow_auto.empty);
        lognum("GPU_SHADOW_AUTO passed=",g_gpu_shadow_auto.passed,"differences=",g_gpu_shadow_auto.differences);
        lognum("GPU_SHADOW_AUTO errors=",g_gpu_shadow_auto.errors,"skipped=",g_gpu_shadow_auto.skipped);
        lognum("GPU_SHADOW_AUTO disabled=",g_gpu_shadow_auto.disabled,"counts_ok=",msi_gpu_shadow_sampler_counts_ok(&g_gpu_shadow_auto));
        lognum("GPU_PUBLISH waves=",g_gpu_publish.waves,"copied=",g_gpu_publish.copied);
        lognum("GPU_PUBLISH changed=",g_gpu_publish.changed,"preserved=",g_gpu_publish.preserved);
        lognum("GPU_PUBLISH rejected=",g_gpu_publish.rejected,NULL,0);
        lognum("GPU_BYPASS sampled_frames=",g_gpu_bypass.frames,"empty=",g_gpu_bypass.empty);
        lognum("GPU_BYPASS attempted=",g_gpu_bypass.attempted,"passed=",g_gpu_bypass.passed);
        lognum("GPU_BYPASS failed=",g_gpu_bypass.failed,"disabled=",g_gpu_bypass.disabled);
        lognum("GPU_BYPASS written=",g_gpu_bypass.written,"changed=",g_gpu_bypass.changed);
    }
    lognum("msiExit focus_isolated_events=",g_focus_filtered_events,"forwarded_messages=",g_guest_messages);
    lognum("MSI_HEAP_TRACKER exit_live=",g_heap_count,"peak_live=",g_heap_peak);
    if(g_vk_requested){lognum("SESSION_VULKAN frames=",g_vk_frames,"skipped=",g_vk_skipped);lognum("SESSION_VULKAN present_ms=",g_vk_ms,"recreates=",g_vk_recreates);}
    clean();
    if(g_gpu_pilot_allocations||g_gpu_pilot_reuses){
        lognum("GPU_PILOT_STORAGE allocations=",g_gpu_pilot_allocations,"reuses=",g_gpu_pilot_reuses);
        lognum("GPU_PILOT_STORAGE releases=",g_gpu_pilot_releases,"live=",g_gpu_pilot_storage.before!=NULL);
    }
    lognum("SHUTDOWN_COMPLETE elapsed_ms=",GetTickCount()-exit_tick,NULL,0);
    lognum("SESSION_TIMING elapsed_ms=",GetTickCount()-g_boot_tick,"render_ms=",g_raster_ms);
    lognum("SESSION_TIMING gdi_ms=",g_preview_ms,"state_ms=",g_state_ms);
    return TRUE;
}
/* Reserve space for the next heap handle BEFORE allocating game memory,
 * so successful game allocations can always be accounted for and freed.
 * The only capacity ceiling is unsigned-size overflow / actual virtual
 * memory exhaustion, not a fixed arbitrary 256-allocation threshold. */
static BOOL grow_heap_tracker(void){
    if(g_heap_count<g_heap_capacity)return TRUE;
    unsigned new_capacity=g_heap_capacity?g_heap_capacity*2u:256u;
    if(new_capacity<=g_heap_capacity ||
       new_capacity>0x7FFFFFFFu/(unsigned)sizeof(void*)){
        logtext("MSI_HEAP_TRACKER cannot grow (address-space limit)");
        return FALSE;
    }
    void **next=(void**)alloc(new_capacity*(unsigned)sizeof(void*));
    if(!next){
        logtext("MSI_HEAP_TRACKER cannot grow (real allocation failure)");
        return FALSE;
    }
    for(unsigned i=0;i<g_heap_count;i++)next[i]=g_heaps[i];
    if(g_heaps)VirtualFree(g_heaps,0,MEM_RELEASE);
    g_heaps=next;
    g_heap_capacity=new_capacity;
    lognum("MSI_HEAP_TRACKER capacity=",g_heap_capacity,"live=",g_heap_count);
    return TRUE;
}
LPBYTE __cdecl msiAllocTextureHeap(LONG Pages){
    gpu_dual_thread_observe();
    gpu_capture_barrier();
    if(g_gpu_profile_active){g_gpu_profile.heap_calls++;gpu_profile_flush();}
    ensure_log();
    if(Pages<=0||Pages>131072){
        lognum("msiAllocTextureHeap INVALID pages=",(unsigned)Pages,NULL,0);
        return NULL;
    }
    if(!grow_heap_tracker())return NULL;
    unsigned n=(unsigned)Pages*4096u;
    void *ptr=alloc(n);
    if(ptr){
        g_last_heap_alloc_tick=GetTickCount();
        g_heaps[g_heap_count++]=ptr;
        if(g_heap_count>g_heap_peak){
            g_heap_peak=g_heap_count;
            if(g_heap_peak==256u||g_heap_peak==512u||g_heap_peak==1024u)
                lognum("MSI_HEAP_TRACKER peak_live=",g_heap_peak,NULL,0);
        }
        lognum("HEAP_ALLOC elapsed_ms=",GetTickCount()-g_boot_tick,"frame=",g_frames);
        lognum("msiAllocTextureHeap pages=",(unsigned)Pages,"bytes=",n);
    }else{
        lognum("msiAllocTextureHeap REAL_OOM pages=",(unsigned)Pages,"live=",g_heap_count);
    }
    sample_game_global_50dcd0("texture_alloc");
    return (LPBYTE)ptr;
}
BOOL __cdecl msiFreeTextureHeap(LPBYTE pTextureHeap){
    gpu_dual_thread_observe();
    gpu_capture_barrier();
    if(g_gpu_profile_active){g_gpu_profile.heap_calls++;gpu_profile_flush();}
    ensure_log();
    if(!pTextureHeap)return FALSE;
    for(unsigned i=0;i<g_heap_count;i++)if(g_heaps[i]==pTextureHeap){
      BOOL ok=VirtualFree(pTextureHeap,0,MEM_RELEASE);
      g_heaps[i]=g_heaps[--g_heap_count];g_heaps[g_heap_count]=NULL;
      sample_game_global_50dcd0("texture_free");
      lognum("msiFreeTextureHeap recognized: live=",g_heap_count,
             "peak=",g_heap_peak);
      return ok;
    }
    logtext("msiFreeTextureHeap UNKNOWN pointer: ignored");return FALSE;
}
BOOL __cdecl msiStartFrame(BOOL ClearRGB,float r,float g,float b,BOOL ClearZ,float z){
    (void)r;(void)g;(void)b;(void)z;
    if(!g_initialized){logtext("msiStartFrame called BEFORE successful msiInit");return FALSE;}
    if(g_frames<5||g_frames%120==0)lognum("msiStartFrame frame=",g_frames,"clear_rgb=",(unsigned)ClearRGB);
    if(ClearRGB&&g_color)zero(g_color,(unsigned)g_info.msiColor.MemSize);
    /* Only record selected frame indices; one-byte ownership writes never
     * affect regular frames' performance or the game's framebuffer. */
    g_manual_capture_active=g_manual_capture_pending!=0;
    if(g_manual_capture_pending)g_manual_capture_pending--;
    g_auto_blue_frame_samples=0;
    /* No automatic debug ownership tracking or BMP capture. */
    g_capture_ownership=g_manual_capture_active;
    gpu_capture_begin();
    gpu_profile_finish();
    if(g_gpu_profile_enabled&&!g_capture_ownership&&g_endframe_depth==0u&&g_frames%60u==0u){
        g_gpu_profile_active=1;g_gpu_profile.sampled++;g_gpu_profile.frame_candidates=0;
        gpu_dual_thread_observe();
    }
    if(g_capture_ownership){
        if(g_ownership)zero(g_ownership,g_ownership_bytes);
        if(g_atlas_provenance)zero(g_atlas_provenance,g_ownership_bytes);
        if(g_world_provenance)zero(g_world_provenance,g_ownership_bytes);
        if(g_shadow_provenance)zero(g_shadow_provenance,g_ownership_bytes);
        if(g_final_texel_source)zero(g_final_texel_source,g_ownership_bytes);
        if(g_final_source_audit)zero(g_final_source_audit,g_ownership_bytes*2u);
        if(g_undithered_reference)zero(g_undithered_reference,g_ownership_bytes*2u);
        if(g_final_light_inputs)zero(g_final_light_inputs,g_ownership_bytes*5u);
        zero(g_world_rgbkey_index_hist,sizeof(g_world_rgbkey_index_hist));
        zero(g_world_key_candidate_hist,sizeof(g_world_key_candidate_hist));
        g_world_index1_preview_attempts=0;
        g_key_alias_bind_reports=0;
        g_world_texture_samples=0;
    }
    /* Tracker is per-frame, never inherited from a preceding frame. */
    if(g_shadow_underlay)zero(g_shadow_underlay,g_ownership_bytes);
    if(ClearZ&&g_depth)zero(g_depth,(unsigned)g_info.msiDepth.MemSize);
    return TRUE;
}
/* Pacing runs after preview_present(), not before rasterization or Windows
 * message processing. A late frame never triggers a catch-up burst.
 * This is an upper bound, not a forced FPS target on slow machines. */
static void frame_limit(void){
    if(g_cap_target_fps==0u)return;
    if(g_cap_qpc_ready){
        MSI_LARGE_INTEGER ts;
        MSI_INT64 interval=(MSI_INT64)((g_cap_frequency+g_cap_target_fps-1u)/g_cap_target_fps);
        if(QueryPerformanceCounter(&ts)){
            if(g_cap_last_tick!=0){
                MSI_INT64 deadline=g_cap_last_tick+interval;
                if(ts.QuadPart<deadline){
                    MSI_INT64 remaining=deadline-ts.QuadPart;
                    /* Leave >=2 ms for coarse Windows timers. One Sleep call
                     * prevents hot spinning through a full ~16.7ms interval. */
                    if(remaining>(MSI_INT64)(2u*(g_cap_frequency/1000u))){
                        MSI_INT64 sleepms=(MSI_INT64)((DWORD)remaining/(g_cap_frequency/1000u))-2LL;
                        if(sleepms>0LL){
                            Sleep((DWORD)sleepms);
                            g_cap_sleep_ms+=(unsigned)sleepms;
                        }
                    }
                    /* Windows/Wine QPC is monotonic in practice. Yield a few
                     * times in the final part; remaining time is normally
                     * under 2ms and prevents >60 FPS despite coarse timers. */
                    {unsigned attempts=0;
                     while(QueryPerformanceCounter(&ts) && ts.QuadPart<deadline){
                         if(++attempts>2000000u)break; /* defensive stuck-QPC */
                         if((attempts&8191u)==0u)Sleep(0);
                     }
                    }
                    ++g_cap_waited_frames;
                }else{
                    ++g_cap_late_frames;
                }
            }
            g_cap_last_tick=ts.QuadPart;
            return;
        }
        g_cap_qpc_ready=0;
        logtext("FPS_CAP QPC failed; switching to conservative GetTickCount pacing");
    }
    /* Fallback for unavailable QPC: conservatively round the target
     * interval up to a whole millisecond. */
    {DWORD now=GetTickCount();
     DWORD interval=(1000u+g_cap_target_fps-1u)/g_cap_target_fps;
     if(g_cap_last_ms){
         DWORD elapsed=now-g_cap_last_ms;
         if(elapsed<interval){Sleep(interval-elapsed);++g_cap_waited_frames;}
         else ++g_cap_late_frames;
     }
     g_cap_last_ms=GetTickCount();
     ++g_cap_fallback_frames;
    }
}
BOOL __cdecl msiEndFrame(BOOL Dump,LONG Frame,BOOL Wait){
    (void)Dump;(void)Frame;(void)Wait;
    if(!g_initialized){logtext("msiEndFrame called BEFORE successful msiInit");return FALSE;}
    gpu_dual_thread_observe();
    if(g_endframe_depth==0u)gpu_profile_finish();
    g_endframe_depth++;
    g_frames++;
    /* No numbered frame snapshots: F12 is the only screenshot trigger. */
    if(g_manual_capture_active){
        char frame[100],coverage[100],atlas[100],world[100],shadow[100],texel[100],audit[100],nodither[100],lighting[100];
        if(next_capture_pair(frame,coverage,atlas,world,shadow,texel,audit,nodither,lighting)){
            capture_frame(frame,0u);
            capture_ownership(coverage);
            capture_atlas_provenance(atlas);
            capture_world_provenance(world);
            capture_shadow_provenance(shadow);
            capture_final_texel_source(texel);
            capture_final_source_audit(audit);
            capture_undithered_reference(nodither);
            capture_final_light_inputs(lighting);
            gpu_capture_save();
            log_opacity_histogram("F12");
            {char line[320];DWORD n=0;
             putstr(line,&n,"MANUAL_CAPTURE shadow=");putstr(line,&n,shadow);
             putstr(line,&n," texel=");putstr(line,&n,texel);
             putstr(line,&n," sourceaudit=");putstr(line,&n,audit);
             putstr(line,&n," nodither=");putstr(line,&n,nodither);
             putstr(line,&n," lighting=");putstr(line,&n,lighting);
             line[n]=0;logtext(line);}
            char message[350];DWORD n=0;
            putstr(message,&n,"MANUAL_CAPTURE pair frame=");
            putstr(message,&n,frame);
            putstr(message,&n," coverage=");
            putstr(message,&n,coverage);
            putstr(message,&n," atlas=");putstr(message,&n,atlas);
            putstr(message,&n," world=");putstr(message,&n,world);
            message[n]=0;logtext(message);
        }
        g_manual_capture_active=0;
        gpu_capture_release();
    }else gpu_capture_auto_finish();
    g_capture_ownership=0;
    sample_game_global_50dcd0("pre_frame_present");
    preview_present();
    sample_game_global_50dcd0("post_frame_present");
    frame_limit(); /* user-configured maximum FPS from [Graphics] */
    if(g_frames%120==0 && g_frames>360 && g_frames-g_last_triangle_frame>=120 && !g_no_geometry_logged){
        g_no_geometry_logged=1;
        lognum("NO_GEOMETRY_BEGIN elapsed_ms=",GetTickCount()-g_boot_tick,"frame=",g_frames);
    }
    if(g_frames<=6 || g_frames%300==0){
        lognum("RENDER_PROGRESS frame=",g_frames,"drawn_triangles=",g_drawn_triangles);
        lognum("RENDER_PROGRESS pixels=",g_drawn_pixels,"missing_textures=",g_missing_textures);
        lognum("RENDER_UPLOAD textures=",g_tex_uploads,"palettes=",g_palette_uploads);
        lognum("CACHE_UPLOAD_WHILE_DISABLED textures=",g_disabled_tex_uploads,"palettes=",g_disabled_lut_uploads);
        lognum("CACHE_DIRECT_READS triangles=",g_direct_cache_reads,NULL,0);
        lognum("RENDER_REJECTED triangles=",g_rejected_triangles,"rasterized_pixels=",g_drawn_pixels);
        lognum("VERTEX_REGION_CHECK reused_queries=",g_vertex_region_reuses,
               "fallback_queries=",g_vertex_region_fallbacks);
        lognum("PERSPECTIVE_UV enabled=",(unsigned)g_perspective_enabled,"triangles=",g_perspective_triangles);
        lognum("PERSPECTIVE_UV adjusted_pixels=",g_perspective_pixels,"varying_invw_world_tris=",g_world_varying_invw);
        lognum("PERSPECTIVE_UV constant_invw_world_tris=",g_world_constant_invw,"invalid_invw_world_tris=",g_world_invalid_invw);
        lognum("WORLD_BILINEAR enabled=",(unsigned)g_world_bilinear_enabled," triangles=",g_world_bilinear_triangles);
        lognum("WORLD_BILINEAR filtered_samples=",g_world_bilinear_samples,"changed_pixels=",g_world_bilinear_changed);
        lognum("WORLD_BILINEAR keyed_neighbor_bypass=",g_world_bilinear_keyed_bypass,NULL,0);
        lognum("WORLD_BILINEAR palette_cached_triangles=",g_world_bilinear_palette_triangles,
               " palette_cached_samples=",g_world_bilinear_palette_samples);
        lognum("WORLD_BILINEAR uniform_neighborhood_samples=",g_world_bilinear_uniform_samples,NULL,0);
        lognum("WORLD_BLUE_KEY samples=",g_world_blue_samples,"transparent_state_samples=",g_world_blue_eligible);
        lognum("WORLD_BLUE_KEY skipped=",g_world_blue_skipped,"enabled=",(unsigned)g_world_blue_key_enabled);
        lognum("INDEXED_KEY candidates=",g_index_key_candidates,"skipped=",g_index_key_skipped);
        lognum("INDEXED_KEY blue_candidates=",g_index_key_blue_candidates,"other_candidates=",g_index_key_other_candidates);
        lognum("INDEXED_KEY enabled=",(unsigned)g_index_key_enabled,NULL,0);
        lognum("ICON01_OPAQUE enabled=",(unsigned)g_icon01_opaque_enabled,
               " verified_uploads=",g_icon01_verified_uploads);
        lognum("ICON01_OPAQUE source_index241_opaque_attempts=",g_icon01_opaque_samples,
               " active_cache_offset=",g_icon01_cache_offset);
        lognum("SHADOW_BLEND dark_flat_pixels=",g_shadow_dark_candidates,"blended_pixels=",g_shadow_dark_blended);
        lognum("SHADOW_BLEND enabled=",(unsigned)g_shadow_blend_enabled,NULL,0);
        lognum("SHADOW_UNDERLAY candidates_with_texture=",g_shadow_underlay_pixels,
               "no_texture=",g_shadow_without_underlay);
        lognum("SHADOW_UNDERLAY opaque_texture=",g_shadow_underlay_opaque,
               "transparent_texture=",g_shadow_underlay_transparent);
        lognum("SHADOW_UNDERLAY blended_pixels=",g_shadow_underlay_blended,
               "enabled=",(unsigned)g_shadow_underlay_enabled);
        lognum("SHADOW_ONCE blended_pixels=",g_shadow_once_blended,
               "repeated_samples=",g_shadow_once_repeated);
        lognum("SHADOW_ONCE untextured_base=",g_shadow_once_no_base,
               "other_surface_resets=",g_shadow_once_nonshadow_resets);
        lognum("SHADOW_ONCE enabled=",(unsigned)g_shadow_once_enabled,NULL,0);
        lognum("RENDER_ZPLANES calls=",g_zplanes,"drawn=",g_drawn_zplanes);
        lognum("RENDER_ZPLANES pixels=",g_drawn_zpixels,"rejected=",g_rejected_zplanes);
        lognum("ZPLANE_BLUE_KEY rgb565=0x00FF skipped_pixels=",g_zplane_blue_key_skips,NULL,0);
        lognum("LARGE_Z_ATLAS triangles=",g_sparse_atlas_triangles,"drawn_pixels=",g_sparse_atlas_drawn_pixels);
        lognum("LARGE_Z_ATLAS missing_samples=",g_sparse_atlas_unmapped_pixels,"direct_nonzero_samples=",g_sparse_atlas_direct_pixels);
        lognum("LARGE_Z_ATLAS zero_skips_color_keyed=",g_sparse_atlas_zero_keyed_skips,
               "zero_skips_not_keyed=",g_sparse_atlas_zero_unkeyed_skips);
        lognum("MSI_NATIVE_REQUEST texture_transparency=",g_native_tex_transparency,
               "alpha_mask_nonzero=",g_native_alpha_mask);
        lognum("MSI_NATIVE_REQUEST clamp_u=",g_native_tex_clamp_u,
               "clamp_v=",g_native_tex_clamp_v);
        lognum("MSI_NATIVE_REQUEST depth_enabled=",g_native_depth_enabled,
               "depth_protect=",g_native_depth_protect);
        lognum("MSI_NATIVE_REQUEST color_protect=",g_native_color_protect,NULL,0);
        lognum("MSI_FX_STATES updates=",g_fx_param_updates,"dither=",g_fx_dither_parameters);
        lognum("MSI_FX_STATES modulate=",g_fx_modulate_parameters,"decal=",g_fx_decal_parameters);
        lognum("MSI_FX_STATES dither_textured=",g_fx_dither_textured_states,"modulate_textured=",g_fx_modulate_textured_states);
        lognum("MSI_FX_TRIANGLES dither=",g_fx_dither_triangles,"modulate=",g_fx_modulate_triangles);
        lognum("MSI_FX_TRIANGLES decal=",g_fx_decal_triangles,NULL,0);
        lognum("MSI_FX_ZPLANES dither=",g_fx_dither_zplanes,"modulate=",g_fx_modulate_zplanes);
        lognum("MSI_FX_ZPLANES decal=",g_fx_decal_zplanes,NULL,0);
        lognum("MSI_MODULATE triangles=",g_modulate_triangles,"pixels=",g_modulate_pixels);
        lognum("MSI_MODULATE varying_triangles=",g_modulate_varying,"enabled=",(unsigned)g_msi_modulate_enabled);
        lognum("MSI_DITHER candidates=",g_dither_candidates,"changed_pixels=",g_dither_changed);
        lognum("MSI_DITHER bypassed_textured_pixels=",g_dither_bypassed,"enabled=",(unsigned)g_msi_dither_enabled);
        lognum("FPS_CAP target_fps=",g_cap_target_fps,"waited_frames=",g_cap_waited_frames);
        lognum("FPS_CAP late_frames=",g_cap_late_frames,"fallback_frames=",g_cap_fallback_frames);
        lognum("FPS_CAP slept_ms=",g_cap_sleep_ms,"qpc_active=",(unsigned)g_cap_qpc_ready);
        lognum("PERF_WALL elapsed_ms=",GetTickCount()-g_boot_tick,"frames=",g_frames);
        lognum("PERF_COST render_ms=",g_raster_ms,"gdi_ms=",g_preview_ms);
        lognum("PERF_COST state_ms=",g_state_ms,"triangles=",g_triangles);
    }
    if(g_frames<6||g_frames%120==0)lognum("msiEndFrame frame=",g_frames,"triangles_total=",g_triangles);
    g_endframe_depth--;
    return TRUE;
}
/* Matrox's original MSI95.dll (OEM 1996 binary) has separate branches at
 * msiSetParameters+0x2A (NULL) and +0x47 (0xFFFFFFFF).  The latter is a
 * deliberate sentinel, NOT a T_msiParameters pointer.  Prior probes crashed
 * in diagnostic formatting by dereferencing it.
 * RWSPMYS1.dll deliberately writes 0xDEADBEEF into msiInfo.msiColor.pMem
 * and msiInfo.msiDepth.pMem on unlock (RVA 0x293D / 0x294A).  On subsequent
 * msiSetParameters(NULL), restore their real RAM addresses; unlike previous
 * probes, this models the direct-framebuffer remapping required for CPU blits.
 * No graphics output backend is implemented. */
static BOOL readable_parameters(const T_msiParameters *p){
    if(!p)return FALSE;
    DWORD start=(DWORD)p;
    DWORD len=(DWORD)sizeof(*p);
    if(start<65536u || start>0xffffffffu-len)return FALSE;
    MEMORY_BASIC_INFORMATION32 mem;
    if(VirtualQuery(p,&mem,sizeof(mem))!=sizeof(mem))return FALSE;
    if(mem.State!=MEM_COMMIT || (mem.Protect & PAGE_GUARD) ||
       (mem.Protect & 0xffu)==PAGE_NOACCESS)return FALSE;
    DWORD region=(DWORD)mem.BaseAddress;
    if(start<region || start-region>mem.RegionSize || len>mem.RegionSize-(start-region))return FALSE;
    return TRUE;
}
static BOOL msiSetParameters_impl(T_msiParameters *p){
    if(!g_initialized){logtext("msiSetParameters called BEFORE successful msiInit");return FALSE;}
    gpu_dual_thread_observe();
    g_params++;
    if(!p||p==(T_msiParameters*)0xffffffffu)gpu_capture_barrier();
    if(g_gpu_profile_active){
        g_gpu_defer_strict_state+=g_gpu_defer_strict.pending!=0;
        msi_gpu_defer_barrier(&g_gpu_defer_strict);
        if(!p){
            g_gpu_defer_audit.remaps++;
            g_gpu_defer_audit.remaps_invalidated+=msi_gpu_defer_pointer((DWORD)g_info.msiColor.pMem,
                (DWORD)g_info.msiDepth.pMem,(DWORD)g_color_guest,(DWORD)g_depth)==MSI_DEFER_INVALIDATED;
            g_gpu_profile.remaps++;gpu_profile_flush();
        }
        else if(p==(T_msiParameters*)0xffffffffu){g_gpu_profile.sentinels++;gpu_profile_flush();}
        else g_gpu_profile.state_calls++;
    }
    /* No automatic buffer BMPs; only deliberate F12 pairs are saved. */
    if(p==(T_msiParameters*)0xffffffffu){
        if(g_params<=20 || g_params%200==0)
            lognum("msiSetParameters SPECIAL -1 (original driver handles explicitly) call=",g_params,NULL,0);
        return TRUE;
    }
    if(p==NULL){
        /* RWSPMYS1 overwrites the *returned MSI info* fields with
         * 0xDEADBEEF on unlock. The original MSI driver rebinds on NULL.
         * Without rebind, SPLANETW.EXE uses that stale pointer and faults
         * at 0x00418F2B (rep stosd) after a few frames. */
        DWORD before_color=(DWORD)g_info.msiColor.pMem;
        DWORD before_depth=(DWORD)g_info.msiDepth.pMem;
        g_info.msiColor.pMem=(LPBYTE)g_color_guest;
        g_info.msiDepth.pMem=(LPBYTE)g_depth;
        if(g_params<=25 || ((before_color!=(DWORD)g_color_guest || before_depth!=(DWORD)g_depth) && g_params%200==0)){
            char buf[940]; DWORD n=0;
            putstr(buf,&n,"msiSetParameters NULL REMAP call=");putunsigned(buf,&n,g_params);
            putstr(buf,&n," color ");puthex(buf,&n,before_color);
            putstr(buf,&n," -> ");puthex(buf,&n,(DWORD)g_color_guest);
            putstr(buf,&n," depth ");puthex(buf,&n,before_depth);
            putstr(buf,&n," -> ");puthex(buf,&n,(DWORD)g_depth);
            buf[n]=0; logtext(buf);
        }
        return TRUE;
    }
    if(p && !readable_parameters(p)){
        char buf[180];DWORD n=0;
        putstr(buf,&n,"msiSetParameters INVALID pointer=");puthex(buf,&n,(DWORD)p);
        putstr(buf,&n," call=");putunsigned(buf,&n,g_params);
        buf[n]=0;logtext(buf);
        return FALSE;
    }
    /* Count actual OEM requested states only for valid non-null parameters.
     * NULL/-1 calls are platform control operations, not render states. */
    g_fx_param_updates++;
    if(p->msiColor.Dither)g_fx_dither_parameters++;
    if(p->msiTexture.Modulate)g_fx_modulate_parameters++;
    if(p->msiTexture.Decal)g_fx_decal_parameters++;
    if(p->msiTexture.Enable && p->msiColor.Dither)g_fx_dither_textured_states++;
    if(p->msiTexture.Enable && p->msiTexture.Modulate)g_fx_modulate_textured_states++;
    if(p->msiTexture.Enable){
        if(p->msiTexture.Transparency)g_native_tex_transparency++;
        if(p->msiTexture.Transparency && p->msiTexture.KeyingAlphaMask)
            g_native_alpha_mask++;
        if(p->msiTexture.Clamp_u)g_native_tex_clamp_u++;
        if(p->msiTexture.Clamp_v)g_native_tex_clamp_v++;
    }
    if(p->msiDepth.Enable)g_native_depth_enabled++;
    if(p->msiDepth.Protect)g_native_depth_protect++;
    if(p->msiColor.Protect)g_native_color_protect++;
    if(g_native_state_details<16u &&
       ((p->msiTexture.Enable && p->msiTexture.Transparency &&
         p->msiTexture.KeyingAlphaMask) ||
        p->msiDepth.Enable || p->msiDepth.Protect ||
        p->msiColor.Protect || p->msiTexture.Decal)){
        char ns[500];DWORD nn=0;
        putstr(ns,&nn,"MSI_NATIVE_STATE frame=");putunsigned(ns,&nn,g_frames);
        putstr(ns,&nn," texture=");putunsigned(ns,&nn,(unsigned)(p->msiTexture.Enable!=0));
        putstr(ns,&nn," transparent=");putunsigned(ns,&nn,(unsigned)(p->msiTexture.Transparency!=0));
        putstr(ns,&nn," key_color=");puthex(ns,&nn,(unsigned)p->msiTexture.KeyingColor);
        putstr(ns,&nn," key_color_mask=");puthex(ns,&nn,(unsigned)p->msiTexture.KeyingColorMask);
        putstr(ns,&nn," key_alpha=");puthex(ns,&nn,(unsigned)p->msiTexture.KeyingAlpha);
        putstr(ns,&nn," key_alpha_mask=");puthex(ns,&nn,(unsigned)p->msiTexture.KeyingAlphaMask);
        putstr(ns,&nn," depth_enable=");putunsigned(ns,&nn,(unsigned)(p->msiDepth.Enable!=0));
        putstr(ns,&nn," depth_compare=");putunsigned(ns,&nn,(unsigned)p->msiDepth.Compare);
        putstr(ns,&nn," depth_protect=");putunsigned(ns,&nn,(unsigned)(p->msiDepth.Protect!=0));
        putstr(ns,&nn," color_protect=");putunsigned(ns,&nn,(unsigned)(p->msiColor.Protect!=0));
        ns[nn]=0;logtext(ns);g_native_state_details++;
    }
    if(g_fx_param_state_logs<12u || (g_fx_param_updates%100000u)==0){
        char fb[360];DWORD fn=0;
        putstr(fb,&fn,"MSI_FX_PARAMS frame=");putunsigned(fb,&fn,g_frames);
        putstr(fb,&fn," update=");putunsigned(fb,&fn,g_fx_param_updates);
        putstr(fb,&fn," tex=");putunsigned(fb,&fn,(unsigned)(p->msiTexture.Enable!=0));
        putstr(fb,&fn," dither=");putunsigned(fb,&fn,(unsigned)(p->msiColor.Dither!=0));
        putstr(fb,&fn," modulate=");putunsigned(fb,&fn,(unsigned)(p->msiTexture.Modulate!=0));
        putstr(fb,&fn," decal=");putunsigned(fb,&fn,(unsigned)(p->msiTexture.Decal!=0));
        putstr(fb,&fn," alpha_key=");putunsigned(fb,&fn,(unsigned)(p->msiTexture.Transparency!=0));
        fb[fn]=0;logtext(fb);g_fx_param_state_logs++;
    }
    if(g_params<=20||g_params%200==0){
       {char buf[940];DWORD n=0;putstr(buf,&n,"msiSetParameters call=");putunsigned(buf,&n,g_params);
         putstr(buf,&n," tex_enable=");putunsigned(buf,&n,(unsigned)p->msiTexture.Enable);
         putstr(buf,&n," tex_wh=");putunsigned(buf,&n,(unsigned)p->msiTexture.Width);
         putstr(buf,&n,"x");putunsigned(buf,&n,(unsigned)p->msiTexture.Height);
         putstr(buf,&n," tex_planes=");putunsigned(buf,&n,(unsigned)p->msiTexture.Planes);
         putstr(buf,&n," z_enable=");putunsigned(buf,&n,(unsigned)p->msiDepth.Enable);
         putstr(buf,&n," heap_ptr=");puthex(buf,&n,(unsigned)p->msiTexture.pHeap);
         buf[n]=0;logtext(buf);
       }
    }
    /* Record only sampled texture states: the OEM menu toggles textured state
     * dozens of times per frame (v0.9 emitted >120k lines). */
    if(g_params <= 25 || (g_params%10000)==0){
       g_last_tex_enable=(int)p->msiTexture.Enable;
       if(p->msiTexture.Enable)g_last_texture_detail_frame=g_frames;
       char buf[940];DWORD n=0;
       putstr(buf,&n,"MSI_TEXTURE_STATE frame=");putunsigned(buf,&n,g_frames);
       putstr(buf,&n," enabled=");putunsigned(buf,&n,(unsigned)p->msiTexture.Enable);
       putstr(buf,&n," size=");putunsigned(buf,&n,(unsigned)p->msiTexture.Width);
       putstr(buf,&n,"x");putunsigned(buf,&n,(unsigned)p->msiTexture.Height);
       putstr(buf,&n," planes=");putunsigned(buf,&n,(unsigned)p->msiTexture.Planes);
       putstr(buf,&n," ptr=");puthex(buf,&n,(unsigned)p->msiTexture.pMem);
       putstr(buf,&n," heap=");puthex(buf,&n,(unsigned)p->msiTexture.pHeap);
       putstr(buf,&n," lut=");puthex(buf,&n,(unsigned)p->msiTexture.msiLUT.pMem);
       putstr(buf,&n," tex_cache_offset=");putunsigned(buf,&n,(unsigned)p->msiTexture.CacheOffset);
       putstr(buf,&n," lut_cache_offset=");putunsigned(buf,&n,(unsigned)p->msiTexture.msiLUT.CacheOffset);
       putstr(buf,&n," depth=");putunsigned(buf,&n,(unsigned)p->msiDepth.Enable);
       buf[n]=0;logtext(buf);
    }
    /* MSI cache uploads are legal independently of texture Enable.
     * Earlier probes mistakenly discarded valid uploads performed while
     * texturing was disabled; those sprites are selected later by CacheOffset.
     * A NULL source pointer selects an existing cache entry. */
    g_state=*p;
    g_state_valid=1;
    {
        LONG w=p->msiTexture.Width, h=p->msiTexture.Height;
        LONG bpp=p->msiTexture.Planes;
        if(w>0 && w<=2048 && h>0 && h<=2048 && (bpp==4 || bpp==8 || bpp==16)){
            unsigned size=((unsigned)w*(unsigned)h*(unsigned)bpp+7u)/8u;
            if(size<=1024u*1024u && p->msiTexture.pMem &&
               readable_span(p->msiTexture.pMem,size) &&
               p->msiTexture.CacheOffset>=0 &&
               (unsigned)p->msiTexture.CacheOffset<16u*1024u*1024u &&
               size<=16u*1024u*1024u-(unsigned)p->msiTexture.CacheOffset){
                unsigned off=(unsigned)p->msiTexture.CacheOffset;
                BYTE *out=(BYTE*)g_texture+off;
                const BYTE *in=p->msiTexture.pMem;
                for(unsigned j=0;j<size;j++)out[j]=in[j];
                for(unsigned j=off/256u;j<=(off+size-1)/256u;j++)g_texture_uploaded[j]=1;
                /* The cache can be reused: never preserve icon01 identity
                 * after overlapping source bytes have been uploaded. */
                if(g_icon01_cache_offset!=0xffffffffu &&
                   off<g_icon01_cache_offset+16384u &&
                   off+size>g_icon01_cache_offset)
                    g_icon01_cache_offset=0xffffffffu;
                if(g_icon01_opaque_enabled && w==128 && h==128 && bpp==8 &&
                   size==16384u){
                    unsigned hash=2166136261u,n0=0u,n1=0u,n241=0u;
                    for(unsigned k=0;k<16384u;k++){
                        unsigned v=out[k];
                        hash=(hash^v)*16777619u;
                        if(v==0u)n0++;
                        if(v==1u)n1++;
                        if(v==241u)n241++;
                    }
                    if(hash==0x35107cbau && n0==0u && n1==0u &&
                       n241==6993u){
                        g_icon01_cache_offset=off;
                        g_icon01_verified_uploads++;
                        lognum("ICON01_OPAQUE original_titlem=icon01_FINGERPRINT_MATCH cache_offset=",
                               off," uploads=",g_icon01_verified_uploads);
                    }
                }
                g_tex_uploads++;
                if(!p->msiTexture.Enable)g_disabled_tex_uploads++;
                if(g_tex_uploads<=8 || (g_tex_uploads%10000u)==0){
                    char msg[210];DWORD n=0;
                    putstr(msg,&n,"MSI_CACHE_UPLOAD tex_offset=");putunsigned(msg,&n,off);
                    putstr(msg,&n," bytes=");putunsigned(msg,&n,size);
                    putstr(msg,&n," plane_bits=");putunsigned(msg,&n,(unsigned)bpp);
                    msg[n]=0;logtext(msg);
                }
            }
            if((bpp==4||bpp==8) && p->msiTexture.msiLUT.pMem){
                unsigned bytes=(bpp==8?512u:32u);
                LONG offset=p->msiTexture.msiLUT.CacheOffset;
                if(offset>=0 && (unsigned)offset<=16u*1024u*1024u-bytes &&
                   readable_span(p->msiTexture.msiLUT.pMem,bytes)){
                   const BYTE*in=p->msiTexture.msiLUT.pMem;
                   BYTE*out=(BYTE*)g_texture+(unsigned)offset;
                   for(unsigned j=0;j<bytes;j++)out[j]=in[j];
                   for(unsigned j=(unsigned)offset/256u;j<=((unsigned)offset+bytes-1)/256u;j++)g_texture_uploaded[j]=1;
                   g_palette_uploads++;
                   if(!p->msiTexture.Enable)g_disabled_lut_uploads++;
                   if(g_palette_uploads<=8 || (g_palette_uploads%10000u)==0){
                       char msg[210];DWORD n=0;
                       putstr(msg,&n,"MSI_CACHE_UPLOAD lut_offset=");putunsigned(msg,&n,(unsigned)offset);
                       putstr(msg,&n," bytes=");putunsigned(msg,&n,bytes);
                       msg[n]=0;logtext(msg);
                   }
                }
            }
        }
    }
    return TRUE;
}
BOOL __cdecl msiSetParameters(T_msiParameters*p){
    DWORD t=GetTickCount();
    BOOL result=msiSetParameters_impl(p);
    g_state_ms+=GetTickCount()-t;
    return result;
}
/* Pointer inspection is diagnostic-only. Never dereference the original game
 * supplied geometry unless its entire record is in committed memory. */
/* Preserve the original readable_span protection and range semantics.
 * A single VirtualQuery region can cover multiple independent pointers only
 * if EVERY complete record fits in the same committed, readable region. */
static BOOL span_in_region(const void *p,DWORD len,
                           const MEMORY_BASIC_INFORMATION32*mem){
    if(!p)return FALSE;
    DWORD start=(DWORD)p;
    if(start<65536u || start>0xffffffffu-len)return FALSE;
    if(mem->State!=MEM_COMMIT || (mem->Protect & PAGE_GUARD) ||
       (mem->Protect & 0xffu)==PAGE_NOACCESS)return FALSE;
    DWORD base=(DWORD)mem->BaseAddress;
    return start>=base && start-base<=mem->RegionSize &&
           len<=mem->RegionSize-(start-base);
}
static BOOL readable_span(const void *p, DWORD len){
    if(!p)return FALSE;
    DWORD start=(DWORD)p;
    if(start<65536u || start>0xffffffffu-len)return FALSE;
    MEMORY_BASIC_INFORMATION32 mem;
    if(VirtualQuery(p,&mem,sizeof(mem))!=sizeof(mem))return FALSE;
    return span_in_region(p,len,&mem);
}
/* Check the first vertex with one fresh VirtualQuery. Reuse its actual
 * memory-region report only for other vertices fully contained there.
 * Distinct/mixed/guarded regions fall back to original individual checks.
 * This saves up to two kernel queries per game triangle without changing
 * memory permissions, original game vertices, or rasterized pixels. */
static BOOL readable_triangle_vertices(const T_msiVertex*a,
                                       const T_msiVertex*b,
                                       const T_msiVertex*c){
    DWORD len=(DWORD)sizeof(*a);
    if(!a || (DWORD)a<65536u || (DWORD)a>0xffffffffu-len)return FALSE;
    MEMORY_BASIC_INFORMATION32 mem;
    if(VirtualQuery(a,&mem,sizeof(mem))!=sizeof(mem))return FALSE;
    if(!span_in_region(a,len,&mem))return FALSE;
    if(span_in_region(b,len,&mem))g_vertex_region_reuses++;
    else {g_vertex_region_fallbacks++;if(!readable_span(b,len))return FALSE;}
    if(span_in_region(c,len,&mem))g_vertex_region_reuses++;
    else {g_vertex_region_fallbacks++;if(!readable_span(c,len))return FALSE;}
    return TRUE;
}
static void put_float_approx(char*buf,DWORD*n,float f){
    if(!(f>=-100000.0f && f<=100000.0f)){putstr(buf,n,"nan/out-of-range");return;}
    if(f<0.0f){putstr(buf,n,"-");f=-f;}
    unsigned part=(unsigned)f;
    unsigned decimal=(unsigned)((f-(float)part)*100.0f+0.5f);
    if(decimal>=100){part++;decimal=0;}
    putunsigned(buf,n,part);putstr(buf,n,".");
    if(decimal<10)putstr(buf,n,"0");putunsigned(buf,n,decimal);
}
static void log_vertex(char*buf,DWORD*n,const char*name,const T_msiVertex*v){
    putstr(buf,n,name);
    putstr(buf,n," xy=(");put_float_approx(buf,n,v->x);putstr(buf,n,",");put_float_approx(buf,n,v->y);
    putstr(buf,n,") uv=(");put_float_approx(buf,n,v->u);putstr(buf,n,",");put_float_approx(buf,n,v->v);
    putstr(buf,n,") z=");put_float_approx(buf,n,v->z);
}
/* Rasterize in the CPU framebuffer and present through the existing GDI path.
 * This is deliberately an experimental stepping stone, not a claim of MSI
 * hardware accuracy (z, perspective, modulation and fog are incomplete). */
/* A few OEM paths can also write into the CPU-addressable cache directly.
 * Detect real nonzero data for small, valid cache regions only, never
 * manufacture font pixels or permit an out-of-range memory read. */
static BOOL cache_nonzero_region(unsigned off,unsigned size){
    if(!g_texture || !size || size>65536u || off>16u*1024u*1024u-size)return FALSE;
    const BYTE*p=(const BYTE*)g_texture+off;
    for(unsigned j=0;j<size;j++)if(p[j])return TRUE;
    return FALSE;
}
static BOOL cache_accessible(unsigned off,unsigned size){
    if(!g_texture || !size || off>16u*1024u*1024u-size)return FALSE;
    if(g_texture_uploaded[off/256u] && g_texture_uploaded[(off+size-1u)/256u])return TRUE;
    if(cache_nonzero_region(off,size)){g_direct_cache_reads++;return TRUE;}
    return FALSE;
}
static int floor_clipped(float x){int i=(int)x;return ((float)i>x)?i-1:i;}
static unsigned clamp_u8(float x){
    if(!(x>=0.0f))return 0;
    if(x<=1.0f)x*=255.0f;
    if(x>=255.0f)return 255;
    return (unsigned)(x+0.5f);
}
/* v0.18: UI and HUD MSI Z-plane sprites use RGB565 0x00FF as a
 * chroma-key. The key is confirmed by identical blue pixels in the loading
 * glyph backgrounds and HUD sprites. Keep the fallback restricted to
 * Z-planes so blue 3D world triangles are never affected. The hardware
 * keying flags are still honored independently. */
/* SDK modulation vertex RGB values are in 0..255, not normalized 0..1. */
static unsigned clamp_mod_255(float f){
    if(!(f>=0.f))return 0;
    if(f>=255.f)return 255;
    return (unsigned)(f+0.5f);
}
static unsigned rgb565(unsigned r,unsigned g,unsigned b){
    return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}
/* Experimental RGB565 ordered dither for Matrox color modulation. The SDK
 * specifies a Dither flag, but not the precise Mystique threshold matrix.
 * All math stays in native 5/6-bit channels: before quantization, a
 * modulated channel is c*modifier/255. Use a reproducible, frame-stable 4x4
 * Bayer threshold to distribute the fractional remainder across pixels.
 * At modifier 255 this is bit-for-bit identical to original RGB565. */
static unsigned modulate_channel_ordered(unsigned channel,unsigned modifier,
                                         unsigned maximum,unsigned threshold){
    unsigned numerator=channel*modifier;
    unsigned q=numerator/255u, rem=numerator%255u;
    if(rem>threshold && q<maximum)q++;
    return q;
}
static unsigned rgb565_modulate_ordered(unsigned color,unsigned mr,unsigned mg,
                                        unsigned mb,unsigned x,unsigned y){
    static const BYTE bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    unsigned matrix=(unsigned)bayer[((y&3u)<<2u)|(x&3u)];
    unsigned threshold=((2u*matrix+1u)*255u)/32u;
    unsigned r=modulate_channel_ordered((color>>11)&31u,mr,31u,threshold);
    unsigned g=modulate_channel_ordered((color>>5)&63u,mg,63u,threshold);
    unsigned b=modulate_channel_ordered(color&31u,mb,31u,threshold);
    return (r<<11)|(g<<5)|b;
}
/* Independently fitted from the owner's original MSI95 1.4 / 86Box
 * synthetic captures: shift-expand channels to eight bits, multiply and
 * truncate /256, then optionally add the measured 2x2 quantization offsets.
 * This is NOT a claim of physical hardware parity. Keep existing coefficient
 * interpolation, keying, alpha and the guest Dither gate unchanged. */
static unsigned rgb888_modulate_565(unsigned color,unsigned mr,unsigned mg,
                                    unsigned mb,unsigned x,unsigned y,int dither){
    static const BYTE ranks[4]={0,3,2,1};
    unsigned rank=dither?(unsigned)ranks[((y&1u)<<1u)|(x&1u)]:0u;
    unsigned r=((((((color>>11)&31u)<<3u)*mr)>>8u)+2u*rank)>>3u;
    unsigned g=((((((color>>5)&63u)<<2u)*mg)>>8u)+rank)>>2u;
    unsigned b=(((((color&31u)<<3u)*mb)>>8u)+2u*rank)>>3u;
    if(r>31u)r=31u;if(g>63u)g=63u;if(b>31u)b=31u;
    return (r<<11u)|(g<<5u)|b;
}
static float cross2(float ax,float ay,float bx,float by){return ax*by-ay*bx;}
/* Same source texel decoder as the nearest path, used only by optional
 * opaque world bilinear after original cache-residency checks. */
static unsigned world_texel_rgb565(const BYTE*tex,const BYTE*lut,unsigned planes,unsigned pos){
    if(planes==16u)return (unsigned)tex[2u*pos]|((unsigned)tex[2u*pos+1u]<<8);
    unsigned idx=(unsigned)tex[pos]; /* only 8bpp supported by this filter */
    return (unsigned)lut[2u*idx]|((unsigned)lut[2u*idx+1u]<<8);
}
/* Caller has already processed the centre texel with the original
 * transparency/index/blue key logic. A 2x2 neighborhood can only be
 * mixed when every tap would also be eligible for opaque drawing.
 * Ignore the experimental black index1/icon01 exemptions here:
 * treating their black as a key is deliberately conservative and keeps
 * silhouettes crisp instead of smearing neighboring black into color. */
static int world_bilinear_tap_opaque(unsigned planes,unsigned raw_index,
                                     unsigned rgb,unsigned transparent,
                                     unsigned key,unsigned mask){
    if(!transparent)return 1;
    if(((rgb^key)&mask)==0u)return 0;
    if(planes==8u && g_index_key_enabled && key<=255u && mask!=0u){
        if((((raw_index^key)&(mask&255u))==0u))return 0;
    }
    if(g_world_blue_key_enabled && rgb==0x00FFu)return 0;
    return 1;
}
static unsigned bilinear_texcoord(int p,unsigned limit,int clamp){
    if(p<0)return clamp?0u:limit-1u;
    if((unsigned)p>=limit)return clamp?limit-1u:0u;
    return (unsigned)p;
}
static void rasterize_triangle_body(const T_msiVertex*a,const T_msiVertex*b,const T_msiVertex*c,BYTE opacity){
    if(!g_state_valid || !g_color || g_info.msiColor.BytesPerPixel!=2 || opacity==0)return;
    float x0=a->x,y0=a->y,x1=b->x,y1=b->y,x2=c->x,y2=c->y;
    if(!(x0>=-10000.f&&x0<=10000.f&&y0>=-10000.f&&y0<=10000.f &&
         x1>=-10000.f&&x1<=10000.f&&y1>=-10000.f&&y1<=10000.f &&
         x2>=-10000.f&&x2<=10000.f&&y2>=-10000.f&&y2<=10000.f)){
        g_rejected_triangles++;return;
    }
    float den=cross2(x1-x0,y1-y0,x2-x0,y2-y0);
    if(den>-0.0001f && den<0.0001f)return;
    float minx=x0,maxx=x0,miny=y0,maxy=y0;
    if(x1<minx)minx=x1;if(x2<minx)minx=x2;
    if(x1>maxx)maxx=x1;if(x2>maxx)maxx=x2;
    if(y1<miny)miny=y1;if(y2<miny)miny=y2;
    if(y1>maxy)maxy=y1;if(y2>maxy)maxy=y2;
    int left=floor_clipped(minx),right=floor_clipped(maxx)+1;
    int top=floor_clipped(miny),bottom=floor_clipped(maxy)+1;
    int width=g_info.msiColor.Pitch/2;
    int height=g_info.msiColor.MemSize/g_info.msiColor.Pitch;
    if(right<=0||bottom<=0||left>=width||top>=height)return;
    if(left<0)left=0;if(top<0)top=0;
    if(right>width)right=width;if(bottom>height)bottom=height;
    if(left>=right||top>=bottom)return;
    int textured=g_state.msiTexture.Enable!=0;
    int modulate=(g_msi_modulate_enabled && textured && !g_zplane_draw_context && g_state.msiTexture.Modulate);
    int mod_varying=0;unsigned fm_r=255,fm_g=255,fm_b=255;
    if(modulate){
        g_modulate_triangles++;
        mod_varying=(a->mr!=b->mr || a->mr!=c->mr || a->mg!=b->mg ||
                     a->mg!=c->mg || a->mb!=b->mb || a->mb!=c->mb);
        if(mod_varying)g_modulate_varying++;
        else {fm_r=clamp_mod_255(a->mr);fm_g=clamp_mod_255(a->mg);fm_b=clamp_mod_255(a->mb);}
    }
    const BYTE* texture=NULL;const BYTE* lut=NULL;
    unsigned texW=0,texH=0,planes=0;
    int sparseAtlas=0;
    int sparseZeroWouldBeKeyed=0; /* diagnostic only; never affects a skip */
    unsigned texOffset=0;
    int perspective=0,world_depth_varying=0,bilinear_triangle=0;
    float aow=a->invW, bow=b->invW, cow=c->invW;
    float auw=0.f, buw=0.f, cuw=0.f, avw=0.f,bvw=0.f,cvw=0.f;
    if(textured){
        LONG tw=g_state.msiTexture.Width, th=g_state.msiTexture.Height;
        LONG bpp=g_state.msiTexture.Planes, off=g_state.msiTexture.CacheOffset;
        if(tw<1||th<1||tw>2048||th>2048||
           (bpp!=4&&bpp!=8&&bpp!=16)||off<0){g_missing_textures++;return;}
        texW=(unsigned)tw;texH=(unsigned)th;planes=(unsigned)bpp;
        if(!g_zplane_draw_context){
            if(aow>0.00000001f && aow<100000000.f &&
               bow>0.00000001f && bow<100000000.f &&
               cow>0.00000001f && cow<100000000.f){
                float mn=aow,mx=aow;
                if(bow<mn)mn=bow;if(cow<mn)mn=cow;
                if(bow>mx)mx=bow;if(cow>mx)mx=cow;
                if(mx-mn>mx*0.00001f){
                    g_world_varying_invw++;
                    world_depth_varying=1;
                    if(g_perspective_enabled){
                        perspective=1;g_perspective_triangles++;
                        auw=a->u*aow;buw=b->u*bow;cuw=c->u*cow;
                        avw=a->v*aow;bvw=b->v*bow;cvw=c->v*cow;
                    }
                } else g_world_constant_invw++;
            } else g_world_invalid_invw++;
        }
        unsigned bytes=(texW*texH*planes+7u)/8u;
        if(bytes>1024u*1024u || (unsigned)off>16u*1024u*1024u-bytes){
            g_missing_textures++;return;
        }
        texOffset=(unsigned)off;
        /* Do NOT demand full residency for a partial sprite atlas.
         * Only Z-planes (2D HUD) use this experimental path; 3D triangles
         * retain v0.18 semantics. The 16-MiB allocation is initialized to
         * zero. Individual texels will be checked before any palette lookup. */
        sparseAtlas=g_zplane_draw_context && bytes>65536u;
        if(sparseAtlas){
            g_sparse_atlas_triangles++;
            if(g_sparse_atlas_log_count<10u){
                char diag[330];DWORD n=0;
                putstr(diag,&n,"LARGE_Z_ATLAS_BIND frame=");putunsigned(diag,&n,g_frames);
                putstr(diag,&n," dimensions=");putunsigned(diag,&n,texW);
                putstr(diag,&n,"x");putunsigned(diag,&n,texH);
                putstr(diag,&n," bpp=");putunsigned(diag,&n,planes);
                putstr(diag,&n," cache_offset=");putunsigned(diag,&n,texOffset);
                putstr(diag,&n," full_bytes=");putunsigned(diag,&n,bytes);
                diag[n]=0;logtext(diag);
                g_sparse_atlas_log_count++;
            }
        }else if(!cache_accessible((unsigned)off,bytes)){
            g_missing_textures++;return;
        }
        texture=(const BYTE*)g_texture+texOffset;
        /* The original nearest center-key path always runs first.
         * Even with game Transparency=1, fully opaque source neighborhoods
         * in perspective-varying 3D world can be filtered safely.
         * Each of four taps is rechecked; keyed neighbors fall back to
         * nearest so sprites, icon01 and silhouettes never gain halos. */
        bilinear_triangle=g_world_bilinear_enabled && world_depth_varying &&
            !g_zplane_draw_context && opacity==100u &&
            (planes==8u || planes==16u) && texW>=2u && texH>=2u;
        if(bilinear_triangle)g_world_bilinear_triangles++;
        if(g_capture_ownership && !g_zplane_draw_context && g_world_texture_samples<8u){
            char binfo[400];DWORD n=0;
            putstr(binfo,&n,"WORLD_TEXTURE_BIND frame=");putunsigned(binfo,&n,g_frames);
            putstr(binfo,&n," wh=");putunsigned(binfo,&n,texW);
            putstr(binfo,&n,"x");putunsigned(binfo,&n,texH);
            putstr(binfo,&n," planes=");putunsigned(binfo,&n,planes);
            putstr(binfo,&n," cache_off=");putunsigned(binfo,&n,texOffset);
            putstr(binfo,&n," transparency=");putunsigned(binfo,&n,(unsigned)g_state.msiTexture.Transparency);
            putstr(binfo,&n," invW_a=");put_float_approx(binfo,&n,aow);
            putstr(binfo,&n," invW_b=");put_float_approx(binfo,&n,bow);
            putstr(binfo,&n," invW_c=");put_float_approx(binfo,&n,cow);
            putstr(binfo,&n," key=");puthex(binfo,&n,g_state.msiTexture.KeyingColor);
            putstr(binfo,&n," mask=");puthex(binfo,&n,g_state.msiTexture.KeyingColorMask);
            binfo[n]=0;logtext(binfo);
            g_world_texture_samples++;
        }
        if(planes!=16){
            LONG loff=g_state.msiTexture.msiLUT.CacheOffset;
            unsigned lutBytes=planes==8?512u:32u;
            if(loff<0 || (unsigned)loff>16u*1024u*1024u-lutBytes ||
               !cache_accessible((unsigned)loff,lutBytes)){
                g_missing_textures++;return;
            }
            lut=(const BYTE*)g_texture+(unsigned)loff;
            /* At most 24 distinct F12 world CLUT+texture+key states.
             * Palettes are reusable independently of texture data, so
             * count *all* RGB565 key aliases and report 0/1/241 values.
             * This is passive; the actual transparency rule below stays
             * exactly as it was. Do not assume unused palette slots are
             * sampled or that key aliases prove hardware semantics. */
            if(planes==8u && !g_zplane_draw_context && g_capture_ownership &&
               g_state.msiTexture.Transparency && g_key_alias_bind_reports<24u){
                unsigned key=(unsigned)g_state.msiTexture.KeyingColor;
                unsigned mask=(unsigned)g_state.msiTexture.KeyingColorMask;
                unsigned fingerprint[4]={texOffset,(unsigned)loff,key,mask};
                int already=0;
                for(unsigned p=0;p<g_key_alias_bind_reports;p++){
                    if(g_key_alias_seen[p][0]==fingerprint[0] &&
                       g_key_alias_seen[p][1]==fingerprint[1] &&
                       g_key_alias_seen[p][2]==fingerprint[2] &&
                       g_key_alias_seen[p][3]==fingerprint[3]){already=1;break;}
                }
                if(!already){
                    for(unsigned j=0;j<4;j++)
                        g_key_alias_seen[g_key_alias_bind_reports][j]=fingerprint[j];
                    ++g_key_alias_bind_reports;
                    unsigned aliases=0;
                    for(unsigned ix=0;ix<256u;ix++){
                        unsigned rgb=(unsigned)lut[2u*ix]|((unsigned)lut[2u*ix+1u]<<8);
                        if((rgb&mask)==(key&mask))aliases++;
                    }
                    unsigned ix0=(unsigned)lut[0]|((unsigned)lut[1]<<8);
                    unsigned ix1=(unsigned)lut[2]|((unsigned)lut[3]<<8);
                    unsigned ix241=(unsigned)lut[482]|((unsigned)lut[483]<<8);
                    char ab[560];DWORD an=0;
                    putstr(ab,&an,"F12_PALETTE_BIND frame=");putunsigned(ab,&an,g_frames);
                    putstr(ab,&an," wh=");putunsigned(ab,&an,texW);
                    putstr(ab,&an,"x");putunsigned(ab,&an,texH);
                    putstr(ab,&an," texture_offset=");putunsigned(ab,&an,texOffset);
                    putstr(ab,&an," lut_offset=");putunsigned(ab,&an,(unsigned)loff);
                    putstr(ab,&an," key=");puthex(ab,&an,key);
                    putstr(ab,&an," mask=");puthex(ab,&an,mask);
                    putstr(ab,&an," palette_key_alias_indices=");putunsigned(ab,&an,aliases);
                    putstr(ab,&an," lut0=");puthex(ab,&an,ix0);
                    putstr(ab,&an," lut1=");puthex(ab,&an,ix1);
                    putstr(ab,&an," lut241=");puthex(ab,&an,ix241);
                    ab[an]=0;logtext(ab);
                    /* Exact original game-resource provenance from the
                     * owner's TITLEM.RWB, measured independently offline.
                     * F12 ONLY: identify the true uploaded texture by a
                     * content hash, never by a volatile heap pointer.
                     * The first 16384 8-bit indices in original icon01
                     * hash to FNV1a32 0x35107cba (6993 pixels index241).
                     * Diagnostic only; no key/discard/draw state changes. */
                    if(texW==128u && texH==128u){
                        unsigned hash=2166136261u;
                        unsigned n0=0,n1=0,n241=0;
                        for(unsigned ix=0;ix<16384u;ix++){
                            unsigned v=(unsigned)texture[ix];
                            hash=(hash^v)*16777619u;
                            if(v==0u)n0++;
                            if(v==1u)n1++;
                            if(v==241u)n241++;
                        }
                        char al[380];DWORD nn=0;
                        putstr(al,&nn,"F12_TEXTURE_ATLAS frame=");putunsigned(al,&nn,g_frames);
                        putstr(al,&nn," texture_offset=");putunsigned(al,&nn,texOffset);
                        putstr(al,&nn," lut_offset=");putunsigned(al,&nn,(unsigned)loff);
                        putstr(al,&nn," fnv1a32=");puthex(al,&nn,hash);
                        putstr(al,&nn," raw_index0=");putunsigned(al,&nn,n0);
                        putstr(al,&nn," raw_index1=");putunsigned(al,&nn,n1);
                        putstr(al,&nn," raw_index241=");putunsigned(al,&nn,n241);
                        if(hash==0x35107cbau && n241==6993u && n1==0u)
                            putstr(al,&nn," original_titlem=icon01_FINGERPRINT_MATCH");
                        al[nn]=0;logtext(al);
                    }
                }
            }
        }
        if(sparseAtlas){
            /* Classify a zero-valued *sample* after palette lookup, but do
             * not assume that source zero is a valid written texel. This
             * distinguishes a reasonable transparent skip from an ambiguous
             * potentially opaque pixel without changing rasterization. */
            unsigned zero_color=planes==16u?0u:
                ((unsigned)lut[0]|((unsigned)lut[1]<<8));
            unsigned mask=(unsigned)g_state.msiTexture.KeyingColorMask;
            int original_key=(g_state.msiTexture.Transparency &&
                ((zero_color & mask)==
                 ((unsigned)g_state.msiTexture.KeyingColor & mask)));
            /* Existing OEM Z-plane blue-key heuristic also skips 0x00FF. */
            sparseZeroWouldBeKeyed=original_key || zero_color==0x00FFu;
        }
        if(!(a->u>=-10000.f&&a->u<=10000.f&&b->u>=-10000.f&&b->u<=10000.f&&
             c->u>=-10000.f&&c->u<=10000.f&&a->v>=-10000.f&&a->v<=10000.f&&
             b->v>=-10000.f&&b->v<=10000.f&&c->v>=-10000.f&&c->v<=10000.f)){
            g_rejected_triangles++;return;
        }
    }
    /* Prepare a per-triangle CLUT+opacity lookup only for sufficiently
     * large, eligible indexed 3D polygons. No global cache or palette
     * generation assumptions: even direct writes to the original CLUT
     * are seen on the next triangle. The small-triangle path retains
     * the prior direct decoding, avoiding 256-entry setup overhead. */
    WORD bilinear_palette_rgb[256];
    BYTE bilinear_palette_opaque[256];
    int bilinear_palette_ready=0;
    if(bilinear_triangle && planes==8u && lut &&
       (unsigned)(right-left)*(unsigned)(bottom-top)>=512u){
        const unsigned trans=(unsigned)g_state.msiTexture.Transparency;
        const unsigned key=(unsigned)g_state.msiTexture.KeyingColor;
        const unsigned mask=(unsigned)g_state.msiTexture.KeyingColorMask;
        for(unsigned index=0;index<256u;index++){
            unsigned rgb=(unsigned)lut[2u*index]|
                         ((unsigned)lut[2u*index+1u]<<8);
            bilinear_palette_rgb[index]=(WORD)rgb;
            bilinear_palette_opaque[index]=(BYTE)world_bilinear_tap_opaque(
                8u,index,rgb,trans,key,mask);
        }
        bilinear_palette_ready=1;
        g_world_bilinear_palette_triangles++;
    }
    gpu_profile_candidate(a,b,c,left,top,right,bottom,width,height,textured,modulate,perspective,bilinear_triangle,planes,opacity);
    gpu_capture_candidate(a,b,c,left,top,right,bottom,width,height,textured,modulate,perspective,bilinear_triangle,planes,opacity,texW,texH,texOffset,texture,lut);
    if(gpu_bypass_try())return; /* No CPU pixel loop for the committed triangle. */
    MsiGpuCaptureSlot *timing_slot=g_gpu_capture_draw&&!g_gpu_capture_bypass?g_gpu_capture_current:NULL;
    MSI_INT64 loop_begin=timing_slot?gpu_cost_now():0;
    float inv=1.0f/den;
    WORD *dst=(WORD*)g_color;
    unsigned pixelCount=0;
    /* Edge-function gradients are constant across each scanline. This avoids
     * two 2D cross-products per pixel; original texture/alpha math is kept. */
    const float step1=(y2-y0)*inv, step2=-(y1-y0)*inv;
    const float startpx=(float)left+0.5f;
    for(int y=top;y<bottom;y++){
        const float py=(float)y+0.5f;
        float w1=cross2(startpx-x0,py-y0,x2-x0,y2-y0)*inv;
        float w2=cross2(x1-x0,y1-y0,startpx-x0,py-y0)*inv;
        for(int x=left;x<right;x++, w1+=step1, w2+=step2){
            float w0=1.f-w1-w2;
            if(w0<-.00001f||w1<-.00001f||w2<-.00001f)continue;
            unsigned color=0, source_index=0, source_index_valid=0;
            unsigned undithered_color=0u;
            unsigned used_mr=255u,used_mg=255u,used_mb=255u;
            unsigned prelight_rgb565=0u;
            if(textured){
                float u=w0*a->u+w1*b->u+w2*c->u;
                float v=w0*a->v+w1*b->v+w2*c->v;
                if(perspective){
                    float invw=w0*aow+w1*bow+w2*cow;
                    if(invw>0.00000001f && invw<100000000.f){
                        u=(w0*auw+w1*buw+w2*cuw)/invw;
                        v=(w0*avw+w1*bvw+w2*cvw)/invw;
                        g_perspective_pixels++;
                    }
                }
                if(g_state.msiTexture.Clamp_u){if(u<0)u=0;if(u>1)u=1;}
                else {u=u-(float)floor_clipped(u);}
                if(g_state.msiTexture.Clamp_v){if(v<0)v=0;if(v>1)v=1;}
                else {v=v-(float)floor_clipped(v);}
                unsigned tx=(unsigned)(u*(float)texW),ty=(unsigned)(v*(float)texH);
                if(tx>=texW)tx=texW-1;if(ty>=texH)ty=texH-1;
                unsigned pos=ty*texW+tx;
                if(sparseAtlas){
                    /* A sprite reads only its (u,v) subrectangle. A texel is
                     * resident if its 256-byte block was explicitly uploaded
                     * or the game wrote a nonzero texel directly into the
                     * MSI info texture pointer. Never draw untouched zeros
                     * as potentially opaque palette-index-zero pixels. */
                    unsigned rel=planes==16?2u*pos:(planes==8?pos:pos/2u);
                    unsigned absolute=texOffset+rel;
                    int marked=g_texture_uploaded[absolute/256u]!=0;
                    int nonzero=(planes==16) ? (texture[rel]!=0 || texture[rel+1u]!=0) :
                                             (texture[rel]!=0);
                    if(g_capture_ownership && g_atlas_provenance){
                        unsigned screen=(unsigned)y*(unsigned)width+(unsigned)x;
                        g_atlas_provenance[screen]=(BYTE)(marked?
                            (nonzero?1u:2u):(nonzero?3u:4u));
                    }
                    if(!marked && !nonzero){
                        g_sparse_atlas_unmapped_pixels++;
                        if(sparseZeroWouldBeKeyed)g_sparse_atlas_zero_keyed_skips++;
                        else g_sparse_atlas_zero_unkeyed_skips++;
                        if(g_capture_ownership && g_ownership){
                            unsigned j=(unsigned)y*(unsigned)width+(unsigned)x;
                            if(!g_ownership[j])g_ownership[j]=9;
                        }
                        continue;
                    }
                    if(!marked)g_sparse_atlas_direct_pixels++;
                }
                if(planes==16){color=(unsigned)texture[2*pos]|((unsigned)texture[2*pos+1]<<8);}
                else{
                    unsigned ix=planes==8?texture[pos]:((pos&1)?(texture[pos/2]>>4):(texture[pos/2]&15));
                    if(planes==8u){source_index=ix;source_index_valid=1u;}
                    color=(unsigned)lut[2*ix]|((unsigned)lut[2*ix+1]<<8);
                }
                /* Classify world samples before any color-key rejection. */
                if(!g_zplane_draw_context && g_capture_ownership && g_world_provenance){
                    unsigned kind=planes==16u?1u:(planes==8u?
                        ((source_index_valid && source_index==0u)?3u:2u):7u);
                    g_world_provenance[(unsigned)y*(unsigned)width+(unsigned)x]=(BYTE)kind;
                }
                /* Isolated diagnostic-render experiment (NOT on main).
                 * F12 source-index map shows index 1 occupies the missing
                 * black silhouette around three menu portraits. Under the
                 * original RGB565=0 key, both palette indices 1 and 241
                 * become black. Keep index 241 keyed, but let index 1 render
                 * its black palette color when using 128x128 indexed menu
                 * textures. A/B against PR #39; never apply to Z-planes. */
                if(g_capture_ownership && !g_zplane_draw_context &&
                   source_index_valid && g_state.msiTexture.Transparency &&
                   ((color&g_state.msiTexture.KeyingColorMask)==
                    (g_state.msiTexture.KeyingColor&g_state.msiTexture.KeyingColorMask)))
                    g_world_key_candidate_hist[source_index]++;
                int preview_index1_opaque=
                    !g_zplane_draw_context && source_index_valid &&
                    source_index==1u && texW==128u && texH==128u &&
                    color==0u && g_state.msiTexture.Transparency &&
                    g_state.msiTexture.KeyingColor==0u &&
                    g_state.msiTexture.KeyingColorMask==0xffffu;
                /* Original TITLEM icon01 holds 6993 index241 background
                 * texels, confirmed by F12 on native Windows. Restore only
                 * this specific atlas, not global post-CLUT black aliases.
                 * Never touch Z-plane/HUD, 16-bit, 4-bit, or other textures. */
                int preview_icon01_opaque=
                    g_icon01_opaque_enabled && !g_zplane_draw_context &&
                    source_index_valid && source_index==241u &&
                    texW==128u && texH==128u && planes==8u &&
                    texOffset==g_icon01_cache_offset &&
                    color==0u && g_state.msiTexture.Transparency &&
                    g_state.msiTexture.KeyingColor==0u &&
                    g_state.msiTexture.KeyingColorMask==0xffffu;
                if(g_state.msiTexture.Transparency &&
                   ((color&g_state.msiTexture.KeyingColorMask)==
                    (g_state.msiTexture.KeyingColor&g_state.msiTexture.KeyingColorMask)) &&
                   !preview_index1_opaque && !preview_icon01_opaque){
                    if(sparseAtlas && g_capture_ownership && g_atlas_provenance){
                        unsigned screen=(unsigned)y*(unsigned)width+(unsigned)x;
                        g_atlas_provenance[screen]=5u;
                    }
                    if(!g_zplane_draw_context && g_capture_ownership && source_index_valid)
                        g_world_rgbkey_index_hist[source_index]++;
                    if(!g_zplane_draw_context && g_capture_ownership && g_world_provenance){
                        unsigned kind=source_index_valid?
                            (source_index==0u?8u:(source_index==1u?9u:
                             (source_index==241u?10u:11u))):4u;
                        g_world_provenance[(unsigned)y*(unsigned)width+(unsigned)x]=(BYTE)kind;
                    }
                    if(g_capture_ownership && g_ownership){
                        unsigned j=(unsigned)y*(unsigned)width+(unsigned)x;
                        if(!g_ownership[j])g_ownership[j]=(BYTE)(g_zplane_draw_context?8:7);
                    }
                    continue;
                }
                if(preview_index1_opaque && g_capture_ownership){
                    g_world_index1_preview_attempts++;
                    if(g_world_provenance)
                        g_world_provenance[(unsigned)y*(unsigned)width+
                                           (unsigned)x]=12u;
                }
                if(preview_icon01_opaque){
                    g_icon01_opaque_samples++;
                    if(g_capture_ownership && g_world_provenance)
                        g_world_provenance[(unsigned)y*(unsigned)width+
                                           (unsigned)x]=13u;
                }
                /* The 8-bit MSI interface has a palette-index ambiguity:
                 * source pixel index zero can map through the CLUT to blue
                 * RGB565 0x00FF. Record source index for every blue texel,
                 * and optionally perform keying before LUT conversion.
                 * This is NOT applied to Z-planes or to opaque world assets. */
                if(source_index_valid && !g_zplane_draw_context &&
                   g_state.msiTexture.Transparency){
                    unsigned key=(unsigned)g_state.msiTexture.KeyingColor;
                    unsigned mask=(unsigned)g_state.msiTexture.KeyingColorMask;
                    int index_match=(key<=255u && mask!=0u &&
                         (((source_index^key)&(mask&255u))==0u));
                    if(index_match){
                        g_index_key_candidates++;
                        if(color==0x00FFu)g_index_key_blue_candidates++;
                        else g_index_key_other_candidates++;
                    }
                    if((color==0x00FFu || index_match) && g_index_key_debug<12u){
                        char diag[410];DWORD n=0;
                        putstr(diag,&n,"INDEXED_KEY_SAMPLE frame=");putunsigned(diag,&n,g_frames);
                        putstr(diag,&n," index=");putunsigned(diag,&n,source_index);
                        putstr(diag,&n," rgb=");puthex(diag,&n,color);
                        putstr(diag,&n," palette_zero=");
                        puthex(diag,&n,(unsigned)lut[0]|((unsigned)lut[1]<<8));
                        putstr(diag,&n," key=");puthex(diag,&n,key);
                        putstr(diag,&n," mask=");puthex(diag,&n,mask);
                        putstr(diag,&n," index_match=");putunsigned(diag,&n,(unsigned)index_match);
                        diag[n]=0;logtext(diag);g_index_key_debug++;
                    }
                    if(index_match && g_index_key_enabled){
                        if(g_capture_ownership && g_world_provenance)
                            g_world_provenance[(unsigned)y*(unsigned)width+(unsigned)x]=5u;
                        g_index_key_skipped++;
                        if(g_capture_ownership && g_ownership){
                            unsigned j=(unsigned)y*(unsigned)width+(unsigned)x;
                            if(!g_ownership[j])g_ownership[j]=7;
                        }
                        continue;
                    }
                }
                /* Track the exact blue seen in world billboards. An opt-in
                 * fallback keys only when this world texture has MSI
                 * Transparency enabled, never for opaque blue world assets. */
                if(!g_zplane_draw_context && color==0x00FFu){
                    g_world_blue_samples++;
                    if(g_state.msiTexture.Transparency){
                        g_auto_blue_frame_samples++;
                        g_world_blue_eligible++;
                        /* A source sample is recorded BEFORE applying the
                         * blue-key fallback. Keep an independent discard
                         * map so later polygons cannot overwrite its history. */
                        if(g_frames>=1200u && !g_auto_blue_first_captured &&
                           width==640 && height==400){
                            if(g_blue_mask_frame!=g_frames){
                                g_blue_mask_frame=g_frames;
                                zero(g_blue_discard_mask,sizeof(g_blue_discard_mask));
                                g_blue_atlas_valid=0;
                                if(texW<=256u && texH<=256u &&
                                   (planes==8u || planes==16u) &&
                                   texW*texH*(planes/8u)<=sizeof(g_blue_atlas_texels)){
                                    unsigned len=texW*texH*(planes/8u);
                                    for(unsigned z=0;z<len;z++)g_blue_atlas_texels[z]=texture[z];
                                    if(planes==8u && lut){
                                        for(unsigned z=0;z<512u;z++)g_blue_atlas_palette[z]=lut[z];
                                    }
                                    g_blue_atlas_width=texW;g_blue_atlas_height=texH;
                                    g_blue_atlas_planes=planes;
                                    g_blue_atlas_tex_offset=texOffset;
                                    g_blue_atlas_lut_offset=(unsigned)g_state.msiTexture.msiLUT.CacheOffset;
                                    g_blue_atlas_key=(unsigned)g_state.msiTexture.KeyingColor;
                                    g_blue_atlas_key_mask=(unsigned)g_state.msiTexture.KeyingColorMask;
                                    g_blue_atlas_valid=1;
                                }
                            }
                            g_blue_discard_mask[(unsigned)y*640u+(unsigned)x]=1;
                        }
                        if(g_world_blue_diagnostic_count<8u){
                            char msg[290];DWORD n=0;
                            putstr(msg,&n,"WORLD_BLUE_SAMPLE frame=");putunsigned(msg,&n,g_frames);
                            putstr(msg,&n," tex_offset=");putunsigned(msg,&n,texOffset);
                            putstr(msg,&n," wh=");putunsigned(msg,&n,texW);
                            putstr(msg,&n,"x");putunsigned(msg,&n,texH);
                            putstr(msg,&n," key=");puthex(msg,&n,g_state.msiTexture.KeyingColor);
                            putstr(msg,&n," mask=");puthex(msg,&n,g_state.msiTexture.KeyingColorMask);
                            msg[n]=0;logtext(msg);g_world_blue_diagnostic_count++;
                        }
                        if(g_world_blue_key_enabled){
                            if(g_capture_ownership && g_world_provenance)
                                g_world_provenance[(unsigned)y*(unsigned)width+(unsigned)x]=6u;
                            g_world_blue_skipped++;
                            if(g_capture_ownership && g_ownership){
                                unsigned j=(unsigned)y*(unsigned)width+(unsigned)x;
                                if(!g_ownership[j])g_ownership[j]=7;
                            }
                            continue;
                        }
                    }
                }
                /* Experimental MSI Z-plane sprite chroma-key fallback.
                 * Do not apply to world triangles or non-textured primitives. */
                if(g_zplane_draw_context && color==0x00FFu){
                    if(sparseAtlas && g_capture_ownership && g_atlas_provenance){
                        unsigned screen=(unsigned)y*(unsigned)width+(unsigned)x;
                        g_atlas_provenance[screen]=5u;
                    }
                    g_zplane_blue_key_skips++;
                    if(g_capture_ownership && g_ownership){
                        unsigned j=(unsigned)y*(unsigned)width+(unsigned)x;
                        if(!g_ownership[j])g_ownership[j]=8;
                    }
                    continue;
                }
                /* Modern optional bilinear RGB565 for OPAQUE 3D only.
                 * u/v have already undergone the original clamp/wrap.
                 * Texel-center convention (u*width-0.5) preserves original
                 * samples at texel centers and wraps correctly at tile seams.
                 * DO NOT use for 2D, HUD, transparent artwork or 4bpp. */
                if(bilinear_triangle){
                    float sx=u*(float)texW-0.5f,sy=v*(float)texH-0.5f;
                    int ix0=floor_clipped(sx),iy0=floor_clipped(sy);
                    unsigned ix1=bilinear_texcoord(ix0+1,texW,(int)g_state.msiTexture.Clamp_u);
                    unsigned iy1=bilinear_texcoord(iy0+1,texH,(int)g_state.msiTexture.Clamp_v);
                    unsigned ix=bilinear_texcoord(ix0,texW,(int)g_state.msiTexture.Clamp_u);
                    unsigned iy=bilinear_texcoord(iy0,texH,(int)g_state.msiTexture.Clamp_v);
                    float fracx=sx-(float)ix0,fracy=sy-(float)iy0;
                    unsigned wx=(unsigned)(fracx*256.0f+0.5f);
                    unsigned wy=(unsigned)(fracy*256.0f+0.5f);
                    unsigned p00=iy*texW+ix,p10=iy*texW+ix1;
                    unsigned p01=iy1*texW+ix,p11=iy1*texW+ix1;
                    unsigned c00,c10,c01,c11;
                    int four_opaque;
                    if(bilinear_palette_ready){
                        /* Four one-byte indices, four predecoded CLUT
                         * colors and four prechecked opacity flags.
                         * The palette is local to this triangle. */
                        unsigned i00=(unsigned)texture[p00];
                        unsigned i10=(unsigned)texture[p10];
                        unsigned i01=(unsigned)texture[p01];
                        unsigned i11=(unsigned)texture[p11];
                        c00=(unsigned)bilinear_palette_rgb[i00];
                        c10=(unsigned)bilinear_palette_rgb[i10];
                        c01=(unsigned)bilinear_palette_rgb[i01];
                        c11=(unsigned)bilinear_palette_rgb[i11];
                        four_opaque=bilinear_palette_opaque[i00] &&
                                    bilinear_palette_opaque[i10] &&
                                    bilinear_palette_opaque[i01] &&
                                    bilinear_palette_opaque[i11];
                        g_world_bilinear_palette_samples++;
                    }else{
                        c00=world_texel_rgb565(texture,lut,planes,p00);
                        c10=world_texel_rgb565(texture,lut,planes,p10);
                        c01=world_texel_rgb565(texture,lut,planes,p01);
                        c11=world_texel_rgb565(texture,lut,planes,p11);
                        unsigned is_transparent=(unsigned)g_state.msiTexture.Transparency;
                        unsigned key=(unsigned)g_state.msiTexture.KeyingColor;
                        unsigned mask=(unsigned)g_state.msiTexture.KeyingColorMask;
                        unsigned r00=planes==8u?(unsigned)texture[p00]:0u;
                        unsigned r10=planes==8u?(unsigned)texture[p10]:0u;
                        unsigned r01=planes==8u?(unsigned)texture[p01]:0u;
                        unsigned r11=planes==8u?(unsigned)texture[p11]:0u;
                        four_opaque=
                            world_bilinear_tap_opaque(planes,r00,c00,is_transparent,key,mask) &&
                            world_bilinear_tap_opaque(planes,r10,c10,is_transparent,key,mask) &&
                            world_bilinear_tap_opaque(planes,r01,c01,is_transparent,key,mask) &&
                            world_bilinear_tap_opaque(planes,r11,c11,is_transparent,key,mask);
                    }
                    if(four_opaque){
                        /* All four colors equal: interpolation cannot change
                         * RGB565. Keep the same filtered/changed semantics
                         * while avoiding twelve channel multiplications. */
                        unsigned filtered;
                        if(c00==c10 && c00==c01 && c00==c11){
                            filtered=c00;
                            g_world_bilinear_uniform_samples++;
                        }else{
                            filtered=msi_bilinear_mix_rgb565(c00,c10,c01,c11,wx,wy);
                        }
                        g_world_bilinear_samples++;
                        if(filtered!=color)g_world_bilinear_changed++;
                        color=filtered;
                    }else{
                        /* At least one texel is keyed: preserve original
                         * nearest center and never blend an alpha halo. */
                        g_world_bilinear_keyed_bypass++;
                    }
                }
                /* Capture the post-sampling RGB565 input BEFORE optional
                 * modulation/dither/opacity. With original nearest sampling,
                 * this is the original CLUT-resolved (or native RGB565) texel.
                 * Experimental bilinear, when enabled, can alter this input. */
                if(g_capture_ownership && g_final_source_audit)
                    prelight_rgb565=color;
                /* Apply MSI texture color after source-index / RGB565 keying,
                 * preserving the validated transparency behavior. Never
                 * apply to HUD Z-planes or untextured shadow triangles. */
                if(modulate){
                    unsigned mr=fm_r,mg=fm_g,mb=fm_b;
                    if(mod_varying){
                        mr=clamp_mod_255(w0*a->mr+w1*b->mr+w2*c->mr);
                        mg=clamp_mod_255(w0*a->mg+w1*b->mg+w2*c->mg);
                        mb=clamp_mod_255(w0*a->mb+w1*b->mb+w2*c->mb);
                    }
                    used_mr=mr;used_mg=mg;used_mb=mb;
                    unsigned rr=(((color>>11)&31u)*mr+127u)/255u;
                    unsigned gg=(((color>>5)&63u)*mg+127u)/255u;
                    unsigned bb=((color&31u)*mb+127u)/255u;
                    unsigned reference=g_rgb888_lighting_enabled?
                        rgb888_modulate_565(color,mr,mg,mb,0u,0u,0):((rr<<11)|(gg<<5)|bb);
                    if(g_capture_ownership && g_undithered_reference)
                        undithered_color=reference;
                    if(g_msi_dither_enabled && g_state.msiColor.Dither){
                        color=g_rgb888_lighting_enabled?
                            rgb888_modulate_565(color,mr,mg,mb,(unsigned)x,(unsigned)y,1):
                            rgb565_modulate_ordered(color,mr,mg,mb,(unsigned)x,(unsigned)y);
                        g_dither_candidates++;
                        if(color!=reference)g_dither_changed++;
                    }else{
                        color=reference;
                        if(g_msi_dither_enabled)g_dither_bypassed++;
                    }
                    g_modulate_pixels++;
                }
            } else {
                float rr=w0*a->r+w1*b->r+w2*c->r;
                float gg=w0*a->g+w1*b->g+w2*c->g;
                float bb=w0*a->b+w1*b->b+w2*c->b;
                color=rgb565(clamp_u8(rr),clamp_u8(gg),clamp_u8(bb));
            }
            unsigned applied_opacity=opacity;
            unsigned j=(unsigned)y*(unsigned)width+(unsigned)x;
            /* Legacy modes remain byte-for-byte equivalent when SHADOW_ONCE
             * is disabled. Never treat z-plane/HUD triangles as terrain shadows. */
            int candidate=(!g_zplane_draw_context && !textured && color==0x0841u);
            if(g_shadow_once_enabled && candidate && opacity==100u &&
               g_shadow_underlay && g_shadow_underlay[j]==3u){
                /* Already blended against the textured underlay in this frame.
                 * Skipping avoids 50pct alpha applied twice or a later opaque
                 * companion triangle overwriting a previous shadow pass. */
                g_shadow_dark_candidates++;
                g_shadow_once_repeated++;
                continue;
            }
            if(candidate){
                g_shadow_dark_candidates++;
                unsigned underlay=g_shadow_underlay?g_shadow_underlay[j]:0u;
                if(g_shadow_underlay){
                    if(underlay){
                        g_shadow_underlay_pixels++;
                        if(underlay==1u)g_shadow_underlay_opaque++;
                        else g_shadow_underlay_transparent++;
                    } else g_shadow_without_underlay++;
                }
                if(opacity==100u && (g_shadow_blend_enabled ||
                    (g_shadow_underlay_enabled && underlay) ||
                    (g_shadow_once_enabled && (underlay==1u || underlay==2u)))){
                    applied_opacity=50u;
                    g_shadow_dark_blended++;
                    if(g_shadow_underlay_enabled)g_shadow_underlay_blended++;
                    if(g_shadow_once_enabled)g_shadow_once_blended++;
                    if(g_shadow_diagnostic_count<6u){
                        char msg[270];DWORD n=0;
                        putstr(msg,&n,"SHADOW_SAMPLE frame=");putunsigned(msg,&n,g_frames);
                        putstr(msg,&n," xy=(");putunsigned(msg,&n,(unsigned)x);
                        putstr(msg,&n,",");putunsigned(msg,&n,(unsigned)y);
                        putstr(msg,&n,") source=0x0841 base=");puthex(msg,&n,dst[j]);
                        putstr(msg,&n," underlay=");putunsigned(msg,&n,underlay);
                        putstr(msg,&n," output_alpha=50");msg[n]=0;logtext(msg);
                        g_shadow_diagnostic_count++;
                    }
                } else if(g_shadow_once_enabled && underlay==0u) {
                    g_shadow_once_no_base++;
                } else if(g_shadow_underlay_enabled && g_shadow_underlay_debug<6u){
                    char msg[240];DWORD n=0;
                    putstr(msg,&n,"SHADOW_UNDERLAY_SKIP frame=");putunsigned(msg,&n,g_frames);
                    putstr(msg,&n," xy=(");putunsigned(msg,&n,(unsigned)x);
                    putstr(msg,&n,",");putunsigned(msg,&n,(unsigned)y);
                    putstr(msg,&n,") underlay=");putunsigned(msg,&n,underlay);
                    msg[n]=0;logtext(msg);g_shadow_underlay_debug++;
                }
            }
            if(applied_opacity<100){
                unsigned old=dst[y*width+x];
                unsigned ar=applied_opacity,br=100-ar;
                unsigned nr=((((color>>11)&31)*ar)+(((old>>11)&31)*br)+50)/100;
                unsigned ng=((((color>>5)&63)*ar)+(((old>>5)&63)*br)+50)/100;
                unsigned nb=(((color&31)*ar)+((old&31)*br)+50)/100;
                color=(nr<<11)|(ng<<5)|nb;
                if(g_capture_ownership && g_undithered_reference &&
                   !g_zplane_draw_context && textured && modulate &&
                   g_msi_dither_enabled && g_state.msiColor.Dither){
                    /* Same source-over blend against the ACTUAL old dst.
                     * Not a full-frame replay without dither: earlier
                     * writes to old dst may themselves have used dither. */
                    unsigned r0=((((undithered_color>>11)&31u)*ar)+(((old>>11)&31u)*br)+50u)/100u;
                    unsigned g0=((((undithered_color>>5)&63u)*ar)+(((old>>5)&63u)*br)+50u)/100u;
                    unsigned b0=(((undithered_color&31u)*ar)+((old&31u)*br)+50u)/100u;
                    undithered_color=(r0<<11)|(g0<<5)|b0;
                }
            }
            dst[y*width+x]=(WORD)color;
            if(g_gpu_capture_draw&&!g_gpu_capture_bypass)
                g_gpu_capture_current->written[j]=(BYTE)(g_state.msiTexture.Transparency?2u:1u);
            if(g_capture_ownership && g_final_light_inputs){
                BYTE *out=g_final_light_inputs+5u*j;
                if(!g_zplane_draw_context && textured && g_final_source_audit){
                    out[0]=(BYTE)prelight_rgb565;
                    out[1]=(BYTE)(prelight_rgb565>>8u);
                    out[2]=(BYTE)used_mr;
                    out[3]=(BYTE)used_mg;
                    out[4]=(BYTE)used_mb;
                }else{
                    out[0]=out[1]=out[2]=out[3]=out[4]=0u;
                }
            }
            if(g_capture_ownership && g_undithered_reference){
                /* This does not change actual color; later writes must clear
                 * the hypothetical reference just like source attribution. */
                unsigned eligible=!g_zplane_draw_context && textured && modulate &&
                                  g_msi_dither_enabled && g_state.msiColor.Dither;
                g_undithered_reference[j]=(WORD)(eligible?undithered_color:0u);
            }
            if(g_capture_ownership && g_final_source_audit){
                unsigned metadata=0u;
                if(!g_zplane_draw_context && textured){
                    if(planes==8u && source_index_valid)
                        metadata=0x4000u | (source_index & 255u);
                    else if(planes==16u)metadata=0x2000u;
                    else if(planes==4u)metadata=0x1000u;
                    if(metadata){
                        if(prelight_rgb565==0u)metadata|=0x0100u;
                        if(modulate)metadata|=0x0200u;
                        if(modulate && g_msi_dither_enabled && g_state.msiColor.Dither)
                            metadata|=0x0400u;
                        if(applied_opacity<100u)metadata|=0x0800u;
                    }
                }
                /* Later flat/HUD/texture writes overwrite this source tag. */
                g_final_source_audit[j]=(WORD)metadata;
            }
            if(g_capture_ownership && g_final_texel_source){
                /* Only last SUCCESSFUL writer, never a keyed/discarded attempt.
                 * 'black' means actual final RGB565 zero after modulation/alpha,
                 * not that all CLUT source colors with index 1 are zero. */
                unsigned kind=0u;
                if(!g_zplane_draw_context && textured){
                    if(planes==8u && source_index_valid){
                        if(color==0u)
                            kind=source_index==1u?1u:(source_index==241u?2u:
                                 (source_index==0u?3u:4u));
                        else
                            kind=source_index==1u?10u:(source_index==241u?11u:5u);
                    }else if(planes==16u)kind=color==0u?6u:7u;
                    else if(planes==4u)kind=color==0u?8u:9u;
                }
                g_final_texel_source[j]=(BYTE)kind;
            }
            /* Read shadow underlay BEFORE the existing state machine updates
             * it. No source/raster change, and only record F12 final writes. */
            if(g_capture_ownership && g_shadow_provenance){
                unsigned prior=g_shadow_underlay?(unsigned)g_shadow_underlay[j]:0u;
                unsigned kind=0u;
                if(candidate){
                    if(prior==1u||prior==2u)kind=applied_opacity==50u?1u:3u;
                    else if(prior==0u)kind=applied_opacity==100u?2u:4u;
                    else kind=5u;
                }
                g_shadow_provenance[j]=(BYTE)kind;
            }
            if(g_shadow_underlay){
                /* 1 = textured opaque world; 2 = textured, key-transparent
                 * capable world; 3 = dark shadow already blended this frame.
                 * For the new mode only, mark successful shadow writes 3;
                 * a new textured fragment replaces the base (1/2), whereas
                 * other opaque/flat/HUD writes invalidate it (0). */
                if(g_shadow_once_enabled){
                    if(!g_zplane_draw_context && textured)
                        g_shadow_underlay[j]=(BYTE)(g_state.msiTexture.Transparency?2u:1u);
                    else if(candidate && opacity==100u && applied_opacity==50u)
                        g_shadow_underlay[j]=3u;
                    else {
                        if(g_shadow_underlay[j])g_shadow_once_nonshadow_resets++;
                        g_shadow_underlay[j]=0u;
                    }
                }else{
                    /* Original v0.27--0.29 underlay behavior, retained. */
                    g_shadow_underlay[j]=(BYTE)((!g_zplane_draw_context && textured)?
                        (g_state.msiTexture.Transparency?2u:1u):0u);
                }
            }
            if(g_capture_ownership && g_ownership){
                unsigned kind=g_zplane_draw_context?(color?5u:6u):
                             (textured?(color?1u:2u):(color?3u:4u));
                g_ownership[(unsigned)y*(unsigned)width+(unsigned)x]=(BYTE)kind;
            }
            pixelCount++;
            if(sparseAtlas)g_sparse_atlas_drawn_pixels++;
        }
    }
    if(timing_slot){
        DWORD ticks=gpu_cost_elapsed(loop_begin);
        timing_slot->loop_draws++;
        if(ticks==UINT32_MAX||ticks>UINT32_MAX-timing_slot->loop_ticks)
            timing_slot->loop_untimed++;
        else timing_slot->loop_ticks+=ticks;
    }
    if(pixelCount>0)g_drawn_triangles++;
    g_drawn_pixels+=pixelCount;
}
static void rasterize_triangle(const T_msiVertex*a,const T_msiVertex*b,const T_msiVertex*c,BYTE opacity){
    DWORD t=GetTickCount();
    g_gpu_defer_strict_seen=0;
    rasterize_triangle_body(a,b,c,opacity);
    if(g_gpu_profile_active&&!g_zplane_draw_context&&!g_gpu_defer_strict_seen){
        g_gpu_defer_strict_early+=g_gpu_defer_strict.pending!=0;
        msi_gpu_defer_barrier(&g_gpu_defer_strict);
    }
    gpu_capture_draw_complete();
    g_raster_ms+=GetTickCount()-t;
}
BOOL __cdecl msiRenderTriangle(T_msiVertex*a,T_msiVertex*b,T_msiVertex*c,BYTE opacity){
    if(!g_initialized)return FALSE;
    gpu_dual_thread_observe();
    g_triangles++;
    count_opacity(g_triangle_opacity,opacity);
    g_last_triangle_frame=g_frames;
    g_last_triangle_tick=GetTickCount();
    if(g_no_geometry_logged){
       lognum("GEOMETRY_RESUMED elapsed_ms=",g_last_triangle_tick-g_boot_tick,"frame=",g_frames);
       g_no_geometry_logged=0;
    }
    /* Validate once, not again for sampled trace messages. */
    BOOL vertices_ok=readable_triangle_vertices(a,b,c);
    if(g_triangles<=18 || g_triangles%1000==0){
        char buf[940];DWORD n=0;
        putstr(buf,&n,"MSI_TRIANGLE frame=");putunsigned(buf,&n,g_frames);
        putstr(buf,&n," count=");putunsigned(buf,&n,g_triangles);
        putstr(buf,&n," opacity=");putunsigned(buf,&n,(unsigned)opacity);
        if(vertices_ok){
            log_vertex(buf,&n," A",a);log_vertex(buf,&n," B",b);log_vertex(buf,&n," C",c);
        } else putstr(buf,&n," INVALID_VERTEX_POINTER");
        buf[n]=0;logtext(buf);
    }
    if(g_state_valid && g_state.msiColor.Dither)g_fx_dither_triangles++;
    if(g_state_valid && g_state.msiTexture.Modulate)g_fx_modulate_triangles++;
    if(g_state_valid && g_state.msiTexture.Decal)g_fx_decal_triangles++;
    if(g_state_valid && g_state.msiTexture.Enable && g_state.msiTexture.Modulate &&
       g_fx_vertex_color_logs<16u && readable_span(a,sizeof(*a)) &&
       readable_span(b,sizeof(*b)) && readable_span(c,sizeof(*c))){
        char mbuf[360];DWORD mn=0;
        putstr(mbuf,&mn,"MSI_FX_MODULATE_VERTEX frame=");putunsigned(mbuf,&mn,g_frames);
        putstr(mbuf,&mn," mrgb_a=(");put_float_approx(mbuf,&mn,a->mr);
        putstr(mbuf,&mn,",");put_float_approx(mbuf,&mn,a->mg);
        putstr(mbuf,&mn,",");put_float_approx(mbuf,&mn,a->mb);
        putstr(mbuf,&mn,") mrgb_b=(");put_float_approx(mbuf,&mn,b->mr);
        putstr(mbuf,&mn,",");put_float_approx(mbuf,&mn,b->mg);
        putstr(mbuf,&mn,",");put_float_approx(mbuf,&mn,b->mb);
        putstr(mbuf,&mn,") mrgb_c=(");put_float_approx(mbuf,&mn,c->mr);
        putstr(mbuf,&mn,",");put_float_approx(mbuf,&mn,c->mg);
        putstr(mbuf,&mn,",");put_float_approx(mbuf,&mn,c->mb);
        putstr(mbuf,&mn,")");mbuf[mn]=0;logtext(mbuf);g_fx_vertex_color_logs++;
    }
    if(vertices_ok)
        rasterize_triangle(a,b,c,opacity);
    else {
        g_rejected_triangles++;
        if(g_gpu_profile_active){
            g_gpu_defer_strict_early+=g_gpu_defer_strict.pending!=0;
            msi_gpu_defer_barrier(&g_gpu_defer_strict);
        }
    }
    return TRUE;
}
/* MSI95 SDK: a Z-plane is a screen-aligned rectangle whose (u,v,uWidth,vHeight)
 * coordinates are in texture PIXELS, unlike the normalized UVs of triangle
 * vertices. Render it as two ordinary triangles through the established
 * RGB565/CLUT software path. Intentionally defer Z testing, perspective and
 * exact Matrox edge rules to a subsequent experimental stage.
 * The bounds and pointer checks are important because the original RenderWare
 * driver can supply transient records during level changes. */
BOOL __cdecl msiRenderZPlane(T_msiZPlane*p,BYTE opacity){
    if(!g_initialized)return FALSE;
    gpu_dual_thread_observe();
    g_zplanes++;
    gpu_capture_barrier();
    if(g_gpu_profile_active){g_gpu_profile.zplanes++;gpu_profile_flush();}
    count_opacity(g_zplane_opacity,opacity);
    if(g_state_valid && g_state.msiColor.Dither)g_fx_dither_zplanes++;
    if(g_state_valid && g_state.msiTexture.Modulate)g_fx_modulate_zplanes++;
    if(g_state_valid && g_state.msiTexture.Decal)g_fx_decal_zplanes++;
    if(g_zplanes==1u)g_first_zplane_frame=g_frames;
    if(!readable_span(p,sizeof(*p))){
        g_rejected_zplanes++;
        if(g_zplanes<=8)logtext("MSI_ZPLANE INVALID_POINTER: skipped");
        return TRUE;
    }
    if(g_zplanes<=8 || g_zplanes%10000u==0){
        char buf[512];DWORD n=0;
        putstr(buf,&n,"MSI_ZPLANE frame=");putunsigned(buf,&n,g_frames);
        putstr(buf,&n," count=");putunsigned(buf,&n,g_zplanes);
        putstr(buf,&n," xy=(");putunsigned(buf,&n,(unsigned)p->x);
        putstr(buf,&n,",");putunsigned(buf,&n,(unsigned)p->y);
        putstr(buf,&n,") wh=(");putunsigned(buf,&n,(unsigned)p->xWidth);
        putstr(buf,&n,",");putunsigned(buf,&n,(unsigned)p->yHeight);
        putstr(buf,&n,") uv=(");putunsigned(buf,&n,(unsigned)p->u);
        putstr(buf,&n,",");putunsigned(buf,&n,(unsigned)p->v);
        putstr(buf,&n,") uvwh=(");putunsigned(buf,&n,(unsigned)p->uWidth);
        putstr(buf,&n,",");putunsigned(buf,&n,(unsigned)p->vHeight);
        putstr(buf,&n,") tex_enabled=");putunsigned(buf,&n,(unsigned)(g_state_valid&&g_state.msiTexture.Enable));
        putstr(buf,&n," z=");put_float_approx(buf,&n,p->z);
        buf[n]=0;logtext(buf);
        /* The font and loading-message glyphs are Z-planes in 16x16 textures.
         * Diagnose texture/cache misses without inventing missing glyphs. */
        if(g_state_valid){
            const T_msiParameters *sp=&g_state;
            LONG w=sp->msiTexture.Width,h=sp->msiTexture.Height, bpp=sp->msiTexture.Planes;
            LONG off=sp->msiTexture.CacheOffset,loff=sp->msiTexture.msiLUT.CacheOffset;
            unsigned size=0, texok=0,lutok=0;
            if(w>0&&w<=2048&&h>0&&h<=2048&&(bpp==4||bpp==8||bpp==16)){
                size=((unsigned)w*(unsigned)h*(unsigned)bpp+7u)/8u;
                if(size<=1024u*1024u && off>=0 && (unsigned)off<=16u*1024u*1024u-size){
                    texok=cache_accessible((unsigned)off,size);
                }
                if(bpp==16)lutok=1;
                else if(loff>=0 && (unsigned)loff <= 16u*1024u*1024u-(bpp==8?512u:32u))
                    lutok=cache_accessible((unsigned)loff,bpp==8?512u:32u);
            }
            char dbuf[900];DWORD dn=0;
            putstr(dbuf,&dn,"ZPLANE_TEX_DIAG tex=");putunsigned(dbuf,&dn,(unsigned)sp->msiTexture.Enable);
            putstr(dbuf,&dn," wh=");putunsigned(dbuf,&dn,(unsigned)w);
            putstr(dbuf,&dn,"x");putunsigned(dbuf,&dn,(unsigned)h);
            putstr(dbuf,&dn," bpp=");putunsigned(dbuf,&dn,(unsigned)bpp);
            putstr(dbuf,&dn," offset=");putunsigned(dbuf,&dn,(unsigned)off);
            putstr(dbuf,&dn," lutoffset=");putunsigned(dbuf,&dn,(unsigned)loff);
            putstr(dbuf,&dn," tex_cache_present=");putunsigned(dbuf,&dn,texok);
            putstr(dbuf,&dn," lut_cache_present=");putunsigned(dbuf,&dn,lutok);
            putstr(dbuf,&dn," raw_tex_nonzero=");
            putunsigned(dbuf,&dn,size&&off>=0&&size<=65536u&&
                (unsigned)off<=16u*1024u*1024u-size&&
                cache_nonzero_region((unsigned)off,size));
            putstr(dbuf,&dn," raw_lut_nonzero=");
            putunsigned(dbuf,&dn,bpp!=16&&loff>=0&&
                (unsigned)loff<=16u*1024u*1024u-512u&&
                cache_nonzero_region((unsigned)loff,bpp==8?512u:32u));
            putstr(dbuf,&dn," pMem=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.pMem);
            putstr(dbuf,&dn," pHeap=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.pHeap);
            putstr(dbuf,&dn," pLUTMem=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.msiLUT.pMem);
            putstr(dbuf,&dn," pLUTHeap=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.msiLUT.pHeap);
            putstr(dbuf,&dn," transparency=");putunsigned(dbuf,&dn,(unsigned)sp->msiTexture.Transparency);
            putstr(dbuf,&dn," key_color=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.KeyingColor);
            putstr(dbuf,&dn," key_mask=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.KeyingColorMask);
            putstr(dbuf,&dn," key_alpha=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.KeyingAlpha);
            putstr(dbuf,&dn," key_alpha_mask=");puthex(dbuf,&dn,(unsigned)sp->msiTexture.KeyingAlphaMask);
            putstr(dbuf,&dn," opacity=");putunsigned(dbuf,&dn,opacity);
            dbuf[dn]=0;logtext(dbuf);
        }
    }
    if(!g_state_valid || !g_color || opacity==0)return TRUE;
    /* Matrox rectangles are top-left anchored, with positive dimensions. */
    if(p->xWidth<=0 || p->yHeight<=0 || p->xWidth>4096 || p->yHeight>4096 ||
       p->x<-8192 || p->x>8192 || p->y<-8192 || p->y>8192 ||
       p->u<-8192 || p->u>8192 || p->v<-8192 || p->v>8192 ||
       p->uWidth<-8192 || p->uWidth>8192 || p->vHeight<-8192 || p->vHeight>8192){
       g_rejected_zplanes++;return TRUE;
    }
    float tw=1.f, th=1.f;
    if(g_state.msiTexture.Enable){
        tw=(float)g_state.msiTexture.Width;
        th=(float)g_state.msiTexture.Height;
        if(tw<1.f || th<1.f || tw>2048.f || th>2048.f){g_rejected_zplanes++;return TRUE;}
    }
    /* The same struct is used for textured and untextured planes. */
    float u0=(float)p->u/tw, v0=(float)p->v/th;
    float u1=(float)(p->u+p->uWidth)/tw, v1=(float)(p->v+p->vHeight)/th;
    T_msiVertex a={0},b={0},c={0},d={0};
    float x0=(float)p->x,y0=(float)p->y;
    float x1=x0+(float)p->xWidth,y1=y0+(float)p->yHeight;
    a.x=x0;a.y=y0;a.u=u0;a.v=v0;
    b.x=x1;b.y=y0;b.u=u1;b.v=v0;
    c.x=x1;c.y=y1;c.u=u1;c.v=v1;
    d.x=x0;d.y=y1;d.u=u0;d.v=v1;
    a.z=b.z=c.z=d.z=p->z;
    a.r=b.r=c.r=d.r=p->r;
    a.g=b.g=c.g=d.g=p->g;
    a.b=b.b=c.b=d.b=p->b;
    unsigned before=g_drawn_pixels;
    g_zplane_draw_context=1;
    rasterize_triangle(&a,&b,&c,opacity);
    rasterize_triangle(&a,&c,&d,opacity);
    g_zplane_draw_context=0;
    unsigned added=g_drawn_pixels-before;
    if(added){g_drawn_zplanes++;g_drawn_zpixels+=added;}
    return TRUE;
}
BOOL __stdcall DllMain(void*module,DWORD reason,void*reserved){
    (void)reserved;if(reason==1)g_module=module;return TRUE;
}
