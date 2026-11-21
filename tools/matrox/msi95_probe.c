/* Scorched Planet Matrox Mystique — MSI95 *diagnostic* compatibility shim.
 * Stage 0.36: shared INI-configurable frame ceiling at MSI frame boundary; preserve validated effects.
 * Target: 32-bit x86 PE DLL, cdecl exports (1996 MSI95.dll ABI).
 * No proprietary code or game data included.
 * Compile with clang targeting i686-pc-windows-msvc and lld-link; no CRT.
 */
int _fltused = 0; /* MSVC x86 floating-point support marker; no CRT */
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
#define PAGE_EXECUTE_READ 0x20U
#define PAGE_EXECUTE_READWRITE 0x40U
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

__declspec(dllimport) HANDLE __stdcall CreateFileA(const char*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(void*,char*,DWORD);
__declspec(dllimport) DWORD __stdcall GetFileAttributesA(const char*);
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
__declspec(dllimport) void* __stdcall AddVectoredExceptionHandler(unsigned long, void*);
__declspec(dllimport) BOOL __stdcall VirtualProtect(void*,SIZE_T,DWORD,DWORD*);
__declspec(dllimport) BOOL __stdcall FlushInstructionCache(HANDLE,const void*,SIZE_T);
__declspec(dllimport) HANDLE __stdcall GetCurrentProcess(void);
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
/* Large HUD atlases can be only partially populated. Older probes required
 * both extreme texture-cache pages to be uploaded, rejecting *all* pixels
 * of a 512x512 / 1024x1024 image when only a small region was resident.
 * Track subtexel residency so missing regions stay transparent. */
static unsigned g_sparse_atlas_triangles=0;
static unsigned g_sparse_atlas_drawn_pixels=0;
static unsigned g_sparse_atlas_unmapped_pixels=0;
static unsigned g_sparse_atlas_direct_pixels=0;
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
/* F12 only: count original (pre-LUT) 8-bit indices whose resolved RGB565
 * value matched the hardware color key. This is not a pixel-coverage map:
 * overdraw and triangle overlap can count a texel more than once. */
static unsigned g_world_rgbkey_index_hist[256];
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
/* v0.33 per-vertex texture-color modulation, independently selectable.
 * v0.34 ordered RGB565 quantization experiment, independently selectable;
 * it affects ONLY opt-in modulated 3D textures with Dither set in MSI95. */
static int g_msi_modulate_enabled=0,g_msi_dither_enabled=0;
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
/* Real nesting of MSI95 end-of-frame calls: sampled deferral depth=2 in
 * the historical 0046D76D AV, but depth=1 in a comparable safe ESC skip.
 * Count every nested entry; log only with explicit Debug Log=1. */
static unsigned g_nested_endframe_events = 0;
static unsigned g_nested_endframe_max_depth = 0;
static unsigned g_nested_endframe_reports = 0;
static unsigned g_last_forwarded_msg = 0;
static unsigned g_last_forwarded_frame = 0;
static DWORD g_last_forwarded_tick = 0;
/* Read-only, Debug Log=1-only queue of the 32 most recent intentional
 * keyboard/button actions actually forwarded into the OEM game callback.
 * Mouse moves are omitted: they would hide an earlier unpaired ESC/Space
 * action before a delayed scene-transition fault. */
typedef struct {
    DWORD tick,frame,msg,wparam,lparam;
} MSI_ACTION_TRACE;
static MSI_ACTION_TRACE g_action_history[32];
static unsigned g_action_history_next = 0;
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
/* Last *observed* transition, not the exact guest-side write timestamp. */
static DWORD g_global_transition_tick = 0;
static unsigned g_global_transition_frame = 0;
/* Original OEM EXE hash 1f268420... verified against owner's disc:
 * [0x0050DCD0] is a SmackOpen video handle; handle+4/+8 are dimensions,
 * +0x0c is Smack->Frames. Video mode is [0x00655750]==4.
 * Passive snapshots ONLY; do not alter Smacker/video/guest state. */
static DWORD g_smack_last_handle=0;
static DWORD g_smack_last_width=0;
static DWORD g_smack_last_height=0;
static DWORD g_smack_last_frames=0;
static DWORD g_smack_last_nonnull_frame=0;
static DWORD g_smack_last_null_frame=0;
/* Sample every MSI95 frame even though normal trace output is sparse.
 * Matrox's SmackOpen may reuse the same address for different movies:
 * compare frame-count, geometry AND counter restart, not pointer alone. */
static DWORD g_smack_prev_counter=0;
static BOOL g_smack_prev_counter_seen=FALSE;
/* The exact OEM video playback tick resumes at 0x0046D757 after
 * invoking renderer flush 0x004185C0. Read-only stack candidates only:
 * matching a value does NOT prove a live call frame. */
static DWORD g_smack_callback_pending_tick=0;
static BOOL g_video_null_guard_installed=FALSE;
static BYTE *g_video_null_guard_stub=NULL;
static DWORD g_video_null_guard_skips=0;
static unsigned g_smack_pending_reports=0;
static unsigned g_smack_callback_close_reports=0;
static unsigned g_smack_pending_close_reports=0;
static unsigned g_smack_important_transitions=0;
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
        /* Smacker reader at 0046D76D tries to read Smack->Frames at +0c
         * AFTER a now-NULL handle was obtained from the global. Fields here
         * are cached from the last readable handle snapshot, not derefs
         * of a potentially closed/unallocated Smacker handle. */
        {
          char state[350];k=0;
          DWORD mode=0xffffffffu;
          if(readable_span((const void*)0x00655750u,4u))
            mode=*(const volatile DWORD*)0x00655750u;
          putstr(state,&k,"AV_SMACK_PLAYBACK mode=");puthex(state,&k,mode);
          putstr(state,&k," cached_handle=");puthex(state,&k,g_smack_last_handle);
          putstr(state,&k," last_width=");putunsigned(state,&k,g_smack_last_width);
          putstr(state,&k," last_height=");putunsigned(state,&k,g_smack_last_height);
          putstr(state,&k," last_total_frames=");putunsigned(state,&k,g_smack_last_frames);
          putstr(state,&k," last_nonnull_observed_frame=");
          putunsigned(state,&k,g_smack_last_nonnull_frame);
          putstr(state,&k," last_null_observed_frame=");
          putunsigned(state,&k,g_smack_last_null_frame);
          state[k]=0;WriteFile(g_log,state,k,&wr,NULL);
          WriteFile(g_log,"\r\n",2,&wr,NULL);
        }
        {
          char pending[230];k=0;
          putstr(pending,&k,"AV_SMACK_TICK_CALLBACK_CONTEXT candidate=");
          puthex(pending,&k,g_smack_callback_pending_tick);
          putstr(pending,&k," callback_closes=");
          putunsigned(pending,&k,g_smack_callback_close_reports);
          putstr(pending,&k," candidate_closes=");
          putunsigned(pending,&k,g_smack_pending_close_reports);
          putstr(pending,&k," nested_endframe_total=");
          putunsigned(pending,&k,g_nested_endframe_events);
          putstr(pending,&k," max_endframe_depth=");
          putunsigned(pending,&k,g_nested_endframe_max_depth);
          pending[k]=0;WriteFile(g_log,pending,k,&wr,NULL);
          WriteFile(g_log,"\r\n",2,&wr,NULL);
        }
        /* Capture only factual context when this exact verified OEM fault
         * happens. Never change EIP, EDX, guest memory or exception result. */
        if(g_global_transition_tick){
          char ep[250];k=0;
          putstr(ep,&k,"AV_6D76D_GLOBAL_EPOCH observed_change_frame=");
          putunsigned(ep,&k,g_global_transition_frame);
          putstr(ep,&k," age_ms=");
          putunsigned(ep,&k,GetTickCount()-g_global_transition_tick);
          ep[k]=0;WriteFile(g_log,ep,k,&wr,NULL);
          WriteFile(g_log,"\r\n",2,&wr,NULL);
        }
        /* Recent dispatched *action* events, oldest first; unlike
         * AV_INPUT_CONTEXT this survives subsequent WM_MOUSEMOVE traffic. */
        unsigned recorded=g_action_history_next<16u?
            g_action_history_next:16u;
        unsigned action_start=g_action_history_next-recorded;
        for(unsigned seq=action_start;seq<g_action_history_next;seq++){
          const MSI_ACTION_TRACE *entry=&g_action_history[seq%32u];
          char ev[230];k=0;
          putstr(ev,&k,"AV_6D76D_ACTION frame=");
          putunsigned(ev,&k,entry->frame);
          putstr(ev,&k," msg=");puthex(ev,&k,entry->msg);
          putstr(ev,&k," wparam=");puthex(ev,&k,entry->wparam);
          putstr(ev,&k," lparam=");puthex(ev,&k,entry->lparam);
          putstr(ev,&k," age_ms=");
          putunsigned(ev,&k,GetTickCount()-entry->tick);
          ev[k]=0;WriteFile(g_log,ev,k,&wr,NULL);
          WriteFile(g_log,"\r\n",2,&wr,NULL);
        }
        /* The stack is not a reliable unwound call-chain with frame-pointer
         * omission. Report *candidate* in-image words, including their
         * instruction neighborhood, without asserting they are callers. */
        if(ctx->Esp && readable_span((const void*)ctx->Esp,256u)){
          const DWORD *stack_words=(const DWORD*)ctx->Esp;
          static const char digits[]="0123456789ABCDEF";
          unsigned candidates=0;
          for(unsigned word=0;word<64u && candidates<8u;word++){
            DWORD value=stack_words[word];
            if(value<0x00401010u || value>=0x006B7000u)continue;
            if(!readable_span((const void*)(value-12u),24u))continue;
            const BYTE *nearby=(const BYTE*)(value-12u);
            char candidate[220];k=0;
            putstr(candidate,&k,"AV_6D76D_STACK_CODE word=");
            putunsigned(candidate,&k,word);
            putstr(candidate,&k," candidate=");puthex(candidate,&k,value);
            putstr(candidate,&k," bytes_at_candidate_minus12=");
            for(unsigned n=0;n<24u;n++){
              if(n==12u)putstr(candidate,&k," |");
              if(k+4>=sizeof(candidate))break;
              candidate[k++]=' ';
              candidate[k++]=digits[nearby[n]>>4];
              candidate[k++]=digits[nearby[n]&15u];
            }
            candidate[k]=0;WriteFile(g_log,candidate,k,&wr,NULL);
            WriteFile(g_log,"\r\n",2,&wr,NULL);
            candidates++;
          }
        }
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
/* Scan a bounded, readable area of this thread's stack for possible
 * OEM return addresses just before the crashing Smacker frame advance.
 * The scan runs ONLY for actual forwarded Win32 callbacks with Log=1. */
static DWORD find_pending_smack_tick(const void *anchor){
    if(!g_log||!game_global_probe_supported()||!anchor)return 0u;
    DWORD a=((DWORD)anchor)&~3u;
    if(a<65536u || a>0xFFFFC000u)return 0u;
    for(DWORD off=0;off<16384u;off+=256u){
        const DWORD*words=(const DWORD*)(a+off);
        if(!readable_span(words,256u))break;
        for(unsigned i=0;i<64u;i++){
            DWORD w=words[i];
            if(w==0x0046D752u || w==0x0046D757u ||
               w==0x0046D75Cu)return w;
        }
    }
    return 0u;
}

/* EXPERIMENTAL, opt-in, hash-locked EXE memory patch.
 * Never change the original game file or Smacker DLL on disk.
 * Only the original six-byte load at 0046D761 is redirected. */
static BOOL video_guard_exact_signature(void){
    static const BYTE load[6]={0x8b,0x15,0xd0,0xdc,0x50,0x00};
    static const BYTE cmp[5]={0x39,0x42,0x0c,0x72,0x30};
    static const BYTE end[8]={0x5d,0x5f,0x5e,0x5b,0x83,0xc4,0x18,0xc3};
    if((DWORD)GetModuleHandleA(NULL)!=0x00400000u ||
       !game_global_probe_supported() ||
       !readable_span((const void*)0x0046D761u,6u) ||
       !readable_span((const void*)0x0046D76Du,5u) ||
       !readable_span((const void*)0x0046D7D2u,8u))return FALSE;
    const BYTE* addresses[3]={(BYTE*)0x0046D761u,(BYTE*)0x0046D76Du,(BYTE*)0x0046D7D2u};
    const BYTE* signatures[3]={load,cmp,end};
    const unsigned lengths[3]={6u,5u,8u};
    for(unsigned j=0;j<3u;j++)
        for(unsigned i=0;i<lengths[j];i++)
            if(addresses[j][i]!=signatures[j][i])return FALSE;
    return TRUE;
}
static void write_video_guard_rel32(BYTE *src,DWORD base,DWORD destination){
    DWORD delta=destination-(base+5u);
    src[0]=0xe9;
    for(unsigned i=0;i<4u;i++)src[i+1u]=(BYTE)(delta>>(i*8u));
}
static BOOL install_video_null_guard(void){
    if(g_video_null_guard_installed)return TRUE;
    if(!video_guard_exact_signature()){
        logtext("SMACK_NULL_GUARD_REFUSED reason=OEM_signatures_not_recognized");
        return FALSE;
    }
    BYTE *stub=(BYTE*)VirtualAlloc(NULL,4096u,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!stub){logtext("SMACK_NULL_GUARD_REFUSED reason=out_of_memory");return FALSE;}
    /* 0: mov edx,[0050DCD0], 6 bytes
     * 6: test edx,edx, 2 bytes
     * 8: jne +11 => offsets 21..25, 2 bytes
     * 10: inc DWORD PTR [private_counter], 6 bytes
     * 16: jmp 0046D7D2 (existing video-tick epilogue), 5 bytes
     * 21: jmp 0046D767 (resume original normal path), 5 bytes */
    static const BYTE load[6]={0x8b,0x15,0xd0,0xdc,0x50,0x00};
    for(unsigned i=0;i<6u;i++)stub[i]=load[i];
    stub[6]=0x85;stub[7]=0xd2;stub[8]=0x75;stub[9]=0x0b;
    stub[10]=0xff;stub[11]=0x05;
    DWORD countptr=(DWORD)&g_video_null_guard_skips;
    for(unsigned i=0;i<4u;i++)stub[12u+i]=(BYTE)(countptr>>(8u*i));
    write_video_guard_rel32(stub+16u,(DWORD)stub+16u,0x0046D7D2u);
    write_video_guard_rel32(stub+21u,(DWORD)stub+21u,0x0046D767u);
    DWORD previous=0;
    if(!VirtualProtect(stub,4096u,PAGE_EXECUTE_READ,&previous)){
        VirtualFree(stub,0u,MEM_RELEASE);
        logtext("SMACK_NULL_GUARD_REFUSED reason=cannot_protect_trampoline");
        return FALSE;
    }
    BYTE *site=(BYTE*)0x0046D761u;
    if(!VirtualProtect(site,6u,PAGE_EXECUTE_READWRITE,&previous)){
        VirtualFree(stub,0u,MEM_RELEASE);
        logtext("SMACK_NULL_GUARD_REFUSED reason=cannot_protect_game_code");
        return FALSE;
    }
    BYTE patch[6]={0};
    write_video_guard_rel32(patch,(DWORD)site,(DWORD)stub);
    patch[5]=0x90;
    for(unsigned i=0;i<6u;i++)site[i]=patch[i];
    BOOL flushed=FlushInstructionCache(GetCurrentProcess(),site,6u);
    DWORD ignored=0;
    VirtualProtect(site,6u,previous,&ignored);
    if(!flushed){
        DWORD writable=0;
        if(VirtualProtect(site,6u,PAGE_EXECUTE_READWRITE,&writable)){
            for(unsigned i=0;i<6u;i++)site[i]=load[i];
            FlushInstructionCache(GetCurrentProcess(),site,6u);
            VirtualProtect(site,6u,writable,&ignored);
            VirtualFree(stub,0u,MEM_RELEASE);
        }else{
            /* Never free reachable code if rollback cannot be guaranteed. */
            g_video_null_guard_stub=stub;
            g_video_null_guard_installed=TRUE;
        }
        logtext("SMACK_NULL_GUARD_REFUSED reason=flush_failed");
        return FALSE;
    }
    g_video_null_guard_stub=stub;
    g_video_null_guard_installed=TRUE;
    logtext("SMACK_NULL_GUARD_INSTALLED mode=opt_in in_memory=1 resume_valid=0x0046D767 skip_null=0x0046D7D2");
    return TRUE;
}
static void restore_video_null_guard(void){
    if(!g_video_null_guard_installed)return;
    BYTE *site=(BYTE*)0x0046D761u;
    const BYTE old_code[6]={0x8b,0x15,0xd0,0xdc,0x50,0x00};
    if(site[0]!=0xe9||site[5]!=0x90){
        logtext("SMACK_NULL_GUARD_RESTORE_FAILED reason=site_changed");return;
    }
    DWORD before=0,ignored=0;
    if(!VirtualProtect(site,6u,PAGE_EXECUTE_READWRITE,&before)){
        logtext("SMACK_NULL_GUARD_RESTORE_FAILED reason=protect_failed");return;
    }
    for(unsigned i=0;i<6u;i++)site[i]=old_code[i];
    BOOL flushed=FlushInstructionCache(GetCurrentProcess(),site,6u);
    VirtualProtect(site,6u,before,&ignored);
    if(!flushed){logtext("SMACK_NULL_GUARD_RESTORE_FAILED reason=flush_failed");return;}
    g_video_null_guard_installed=FALSE;
    /* The one-page trampoline remains reserved until process shutdown:
     * an in-flight redirected call could still be returning through it. */
    lognum("SMACK_NULL_GUARD_RESTORED null_tick_skips=",g_video_null_guard_skips,NULL,0);
}

static void sample_game_global_50dcd0(const char *phase){
    if(!g_log || !game_global_probe_supported() ||
       !readable_span((const void*)0x0050DCC0u,0x14u))return;
    const volatile DWORD *globals=(const volatile DWORD*)0x0050DCC0u;
    DWORD counter=globals[0],ptr=globals[4];
    BOOL changed=ptr!=g_game_50dcd0_last;
    if(changed){
        g_global_transition_tick=GetTickCount();
        g_global_transition_frame=g_frames;
    }
    DWORD movie_mode=0xffffffffu;
    if(readable_span((const void*)0x00655750u,4u))
        movie_mode=*(const volatile DWORD*)0x00655750u;
    BOOL movie_readable=FALSE;
    DWORD width=0,height=0,frames=0;
    if(ptr && readable_span((const void*)(SIZE_T)ptr,16u)){
        const volatile DWORD *movie=(const volatile DWORD*)(SIZE_T)ptr;
        width=movie[1];height=movie[2];frames=movie[3];
        movie_readable=TRUE;
    }
    BOOL format_changed=movie_readable && g_smack_last_handle==ptr &&
        (width!=g_smack_last_width || height!=g_smack_last_height ||
         frames!=g_smack_last_frames);
    BOOL counter_restarted=movie_readable && g_smack_prev_counter_seen &&
        g_smack_last_handle==ptr && counter<g_smack_prev_counter;
    g_smack_prev_counter=counter;
    g_smack_prev_counter_seen=TRUE;
    /* Always refresh the last known Smacker header, even when a normal
     * frame doesn't emit a line: avoids 120-frame blind spots. */
    if(movie_readable){
        g_smack_last_handle=ptr;
        g_smack_last_width=width;
        g_smack_last_height=height;
        g_smack_last_frames=frames;
        g_smack_last_nonnull_frame=g_frames;
    }else if(changed && !ptr)g_smack_last_null_frame=g_frames;
    BOOL important=changed||format_changed||counter_restarted;
    if(!important && (g_game_50dcd0_reports>=96u ||
       (g_frames%120u!=0u || counter==g_game_50dcc0_last)))return;
    if(important && g_smack_important_transitions>=256u){
        g_game_50dcd0_last=ptr;
        g_game_50dcc0_last=counter;
        return;
    }
    if(changed && !ptr && g_smack_callback_pending_tick){
        g_smack_pending_close_reports++;
        char msg[270];DWORD n=0;
        putstr(msg,&n,"SMACK_CLOSE_WITH_PENDING_TICK frame=");
        putunsigned(msg,&n,g_frames);
        putstr(msg,&n," phase=");putstr(msg,&n,phase);
        putstr(msg,&n," candidate=");puthex(msg,&n,g_smack_callback_pending_tick);
        putstr(msg,&n," callback_depth=");
        putunsigned(msg,&n,g_guest_callback_depth);
        putstr(msg,&n," counter=");putunsigned(msg,&n,counter);
        msg[n]=0;logtext(msg);
    }
    char info[480];DWORD k=0;
    putstr(info,&k,"GAME_GLOBAL_50DCD0 phase=");
    putstr(info,&k,phase);
    putstr(info,&k," frame=");putunsigned(info,&k,g_frames);
    putstr(info,&k," ptr=");puthex(info,&k,ptr);
    putstr(info,&k," prior_ptr=");puthex(info,&k,g_game_50dcd0_last);
    putstr(info,&k," counter_50DCC0=");puthex(info,&k,counter);
    putstr(info,&k," callback_depth=");putunsigned(info,&k,g_guest_callback_depth);
    putstr(info,&k," movie_mode=");puthex(info,&k,movie_mode);
    if(format_changed)putstr(info,&k," smack_format_switch_same_handle=1");
    if(counter_restarted)putstr(info,&k," smack_counter_restarted=1");
    if(movie_readable){
        putstr(info,&k," smack_width=");putunsigned(info,&k,width);
        putstr(info,&k," smack_height=");putunsigned(info,&k,height);
        putstr(info,&k," smack_frames=");putunsigned(info,&k,frames);
    }else if(!ptr && g_smack_last_handle){
        putstr(info,&k," last_smack_handle=");puthex(info,&k,g_smack_last_handle);
        putstr(info,&k," last_smack_frames=");putunsigned(info,&k,g_smack_last_frames);
        putstr(info,&k," last_smack_dimensions=");putunsigned(info,&k,g_smack_last_width);
        putstr(info,&k,"x");putunsigned(info,&k,g_smack_last_height);
    }else if(ptr)putstr(info,&k," smack_fields_unreadable=1");
    info[k]=0;logtext(info);
    g_game_50dcd0_last=ptr;
    g_game_50dcc0_last=counter;
    g_game_50dcd0_reports++;
    if(important)g_smack_important_transitions++;
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
/* GDI presents the original CPU RGB565 framebuffer plus experimental
 * software-rasterized MSI triangles. No exclusive display-mode change. */
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
    int result=StretchDIBits(dc,dx,dy,dstw,dsth,0,0,(int)w,(int)h,g_color,&bi,DIB_RGB_COLORS,SRCCOPY);
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
        if(dc){preview_blit(dc);EndPaint(hwnd,&ps);}
        return 0;
    }
    if(msg==WM_CLOSE){
        logtext("PREVIEW_WINDOW CLOSED by user");
        DestroyWindow(hwnd);return 0;
    }
    if(msg==WM_DESTROY){if(g_preview==hwnd)g_preview=NULL;return 0;}
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
        if(g_log && (msg==WM_KEYDOWN || msg==WM_KEYUP ||
             msg==WM_CHAR || msg==WM_LBUTTONDOWN || msg==WM_LBUTTONUP ||
             msg==WM_RBUTTONDOWN || msg==WM_RBUTTONUP)){
            MSI_ACTION_TRACE *trace=&g_action_history[g_action_history_next%32u];
            trace->tick=g_last_forwarded_tick;
            trace->frame=g_frames;
            trace->msg=msg;
            trace->wparam=(DWORD)wparam;
            trace->lparam=(DWORD)lparam;
            g_action_history_next++;
        }
        /* A callback may close an active Smacker movie while the
         * playback tick is suspended in a rendering call. */
        DWORD stack_anchor=0u;
        DWORD previous_pending_tick=g_smack_callback_pending_tick;
        unsigned nested_before_callback=g_nested_endframe_events;
        DWORD candidate=find_pending_smack_tick(&stack_anchor);
        DWORD before=0xffffffffu;
        if(g_log && game_global_probe_supported() &&
           readable_span((const void*)0x0050DCD0u,4u))
            before=*(const volatile DWORD*)0x0050DCD0u;
        g_smack_callback_pending_tick=candidate;
        g_guest_callback_depth++;
        result=CallWindowProcA(g_guest_window_proc,hwnd,msg,wparam,lparam);
        sample_game_global_50dcd0("post_callback");
        if(g_log && (candidate || before!=0xffffffffu)){
            DWORD after=0xffffffffu;
            if(readable_span((const void*)0x0050DCD0u,4u))
                after=*(const volatile DWORD*)0x0050DCD0u;
            BOOL closed=before!=0u && before!=0xffffffffu && after==0u;
            BOOL action=msg==WM_KEYDOWN||msg==WM_KEYUP||
                        msg==WM_CHAR||msg==WM_LBUTTONDOWN||
                        msg==WM_LBUTTONUP||msg==WM_RBUTTONDOWN||
                        msg==WM_RBUTTONUP;
            if(closed)g_smack_callback_close_reports++;
            if((closed || (candidate && action)) &&
               g_smack_pending_reports<64u){
                char msgbuf[350];DWORD n=0;
                putstr(msgbuf,&n,closed?
                    "SMACK_CALLBACK_CLOSED_VIDEO":
                    "SMACK_CALLBACK_PENDING_TICK");
                putstr(msgbuf,&n," frame=");putunsigned(msgbuf,&n,g_frames);
                putstr(msgbuf,&n," input=");puthex(msgbuf,&n,msg);
                putstr(msgbuf,&n," key=");puthex(msgbuf,&n,(DWORD)wparam);
                putstr(msgbuf,&n," render_depth=");
                putunsigned(msgbuf,&n,g_endframe_depth);
                putstr(msgbuf,&n," nested_ends_during_callback=");
                putunsigned(msgbuf,&n,g_nested_endframe_events-nested_before_callback);
                putstr(msgbuf,&n," candidate=");puthex(msgbuf,&n,candidate);
                putstr(msgbuf,&n," before=");puthex(msgbuf,&n,before);
                putstr(msgbuf,&n," after=");puthex(msgbuf,&n,after);
                msgbuf[n]=0;logtext(msgbuf);
                g_smack_pending_reports++;
            }
        }
        g_guest_callback_depth--;
        g_smack_callback_pending_tick=previous_pending_tick;
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
      putstr(buf,&n," source=640x400 display_aspect=4:3 pixel_aspect=5:6 pillarbox=black; GDI RGB565, input forwarded");
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
    HDC dc=GetDC(g_preview);
    if(dc){preview_blit(dc);ReleaseDC(g_preview,dc);}
    g_preview_ms += GetTickCount()-preview_tick;
    if(g_frames==5||g_frames==60||g_frames==240||g_frames%600==0){
        lognum("PREVIEW_PRESENT frames=",g_frames,"successful_blits=",g_preview_presented);
        lognum("GAME_WINDOW_EVENTS forwarded=",g_guest_messages,"input_msgs=",g_input_messages);
        lognum("FOCUS_FILTER_STATS ignored_for_guest=",g_focus_filtered_events,NULL,0);
        lognum("INPUT_PUMP_STATS nested_deferred=",g_inner_input_pumps_deferred,
               "frame=",g_frames);
    }
}
static void clean(void){
    for(unsigned i=0;i<g_heap_count;i++)if(g_heaps[i])VirtualFree(g_heaps[i],0,MEM_RELEASE);
    if(g_heaps)VirtualFree(g_heaps,0,MEM_RELEASE);
    g_heaps=NULL;
    g_heap_count=0;g_heap_capacity=0;g_heap_peak=0;
    if(g_color)VirtualFree(g_color,0,MEM_RELEASE);
    if(g_depth)VirtualFree(g_depth,0,MEM_RELEASE);
    if(g_texture)VirtualFree(g_texture,0,MEM_RELEASE);
    if(g_ownership)VirtualFree(g_ownership,0,MEM_RELEASE);
    if(g_atlas_provenance)VirtualFree(g_atlas_provenance,0,MEM_RELEASE);
    if(g_world_provenance)VirtualFree(g_world_provenance,0,MEM_RELEASE);
    if(g_shadow_underlay)VirtualFree(g_shadow_underlay,0,MEM_RELEASE);
    g_shadow_underlay=NULL;
    g_ownership=NULL;g_atlas_provenance=NULL;g_world_provenance=NULL;
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
    return GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;
}
static int next_capture_pair(char*frame,char*coverage,char*atlas,char*world){
    for(unsigned tries=0;tries<999999u;tries++){
        if(g_manual_capture_sequence>=999999u)g_manual_capture_sequence=0;
        unsigned serial=++g_manual_capture_sequence;
        capture_name(frame,serial,"_frame.bmp");
        capture_name(coverage,serial,"_coverage.bmp");
        capture_name(atlas,serial,"_atlas.bmp");
        capture_name(world,serial,"_world.bmp");
        int frame_exists=capture_file_present(frame);
        int coverage_exists=capture_file_present(coverage);
        int atlas_exists=capture_file_present(atlas);
        int world_exists=capture_file_present(world);
        if(frame_exists<0||coverage_exists<0||atlas_exists<0||world_exists<0){
            logtext("MANUAL_CAPTURE cannot resolve output directory");
            return FALSE;
        }
        if(!frame_exists&&!coverage_exists&&!atlas_exists&&!world_exists)return TRUE;
    }
    logtext("MANUAL_CAPTURE exhausted available file numbers");
    return FALSE;
}
/* Snapshot the combined CPU + experimental MSI RGB565 framebuffer.
 * BMP uses top-down BI_BITFIELDS. */
static void put16le(BYTE *p,unsigned v){p[0]=(BYTE)v;p[1]=(BYTE)(v>>8);}
static void put32le(BYTE *p,unsigned v){put16le(p,v);put16le(p+2,v>>16);}
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
    unsigned counts[13];
    {volatile unsigned *p=(volatile unsigned*)&counts[0];
     for(unsigned i=0;i<13u;i++)p[i]=0u;}
    /* Keyed index 0=magenta; 1=yellow; 241=violet; other=white;
     * previewed opaque index1 is amber. */
    static const WORD colors[13]={0x0000u,0x07E0u,0xAFE5u,0xFD20u,
                                  0x8010u,0xF800u,0x001Fu,0x07FFu,
                                  0xF81Fu,0xFFE0u,0xA01Fu,0xFFFFu,
                                  0xFC00u};
    for(unsigned y=0;y<height&&ok;y++){
        for(unsigned x=0;x<width;x++){
            unsigned kind=g_world_provenance[y*width+x];
            if(kind>12u)kind=0u;
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
    sl[sn]=0;logtext(sl);
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

/* Exported functions: MSI95 SDK explicitly declares undecorated extern "C". */
T_msiInfo* __cdecl msiInit(LONG Width,LONG Height,LONG Planes,BOOL ZBuffer,BOOL Debug,WNDPROC proc){
    ensure_log();log_runtime_host();loginit(Width,Height,Planes,ZBuffer,Debug);
    logtext("MATROX_INDEX1_OPAQUE_PREVIEW enabled=1 scope=world_8bpp_128x128_black_rgb565_key; ONLY experimental stacked PR");
    logtext("SMACK_LIFECYCLE_PROBE enabled=1 read_only=1 known_oem_mov_and_cmp_signatures=1");
    logtext("SMACK_PENDING_TICK_PROBE enabled=1 return_candidates=0x0046D752,0x0046D757,0x0046D75C read_only=1");
    logtext("MSI_NESTED_ENDFRAME_PROBE enabled=1 max_depth_and_callback_lifecycle=1 read_only=1");
    {char flag[4]={0};
     DWORD n=GetEnvironmentVariableA("MATROX_MSI_SMACK_GUARD",flag,sizeof(flag));
     if(n==1u && flag[0]=='1'){
         if(!install_video_null_guard()){
             logtext("msiInit FAILED: requested Smacker null-tick guard not installed");
             return NULL;
         }
     }else logtext("SMACK_NULL_GUARD disabled_default=1");
    }
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
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_PERSPECTIVE",setting,sizeof(setting));
      g_perspective_enabled=(n==1u && setting[0]=='1');}
    logtext(g_perspective_enabled?"PERSPECTIVE_MODE enabled (world triangles only)":"PERSPECTIVE_MODE disabled (v0.19 reference rasterization)");
    {char setting[8]={0};DWORD n=GetEnvironmentVariableA("MATROX_MSI_MODULATE",setting,sizeof(setting));
      g_msi_modulate_enabled=(n==1u && setting[0]=='1');}
    logtext(g_msi_modulate_enabled?"MSI_MODULATE enabled (opt-in per-vertex RGB, textured triangles only; Z-plane HUD excluded)":
        "MSI_MODULATE disabled (v0.32 reference textured colors)");
    {char setting[8];DWORD n=GetEnvironmentVariableA("MATROX_MSI_DITHER",setting,sizeof(setting));
      g_msi_dither_enabled=(n==1u && setting[0]=='1');}
    logtext(g_msi_dither_enabled?"MSI_DITHER enabled (experimental 4x4 ordered RGB565, modulated 3D world only)":
            "MSI_DITHER disabled (v0.33 modulation quantization)");
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
    g_color=alloc(framesize);g_depth=alloc(zsize);g_texture=alloc(texsize);
    g_ownership_bytes=(unsigned)Width*(unsigned)Height;
    g_ownership=(BYTE*)alloc(g_ownership_bytes);
    g_atlas_provenance=(BYTE*)alloc(g_ownership_bytes);
    g_world_provenance=(BYTE*)alloc(g_ownership_bytes);
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
    g_info.msiColor.Pitch=(LONG)pitch;g_info.msiColor.pMem=(LPBYTE)g_color;g_info.msiColor.MemSize=(LONG)framesize;
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
    lognum("MSI_NESTED_ENDFRAME_STATS total=",g_nested_endframe_events,
           "max_depth=",g_nested_endframe_max_depth);
    if(g_video_null_guard_installed)
        lognum("SMACK_NULL_GUARD_SUMMARY null_tick_skips=",g_video_null_guard_skips,NULL,0);
    lognum("msiExit total_zplanes=",g_zplanes,"parameter_updates=",g_params);
    lognum("msiExit focus_isolated_events=",g_focus_filtered_events,"forwarded_messages=",g_guest_messages);
    lognum("MSI_HEAP_TRACKER exit_live=",g_heap_count,"peak_live=",g_heap_peak);
    clean();
    lognum("SHUTDOWN_COMPLETE elapsed_ms=",GetTickCount()-exit_tick,NULL,0);
    lognum("SESSION_TIMING elapsed_ms=",GetTickCount()-g_boot_tick,"render_ms=",g_raster_ms);
    lognum("SESSION_TIMING gdi_ms=",g_preview_ms,"state_ms=",g_state_ms);
    restore_video_null_guard();
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
    if(g_capture_ownership){
        if(g_ownership)zero(g_ownership,g_ownership_bytes);
        if(g_atlas_provenance)zero(g_atlas_provenance,g_ownership_bytes);
        if(g_world_provenance)zero(g_world_provenance,g_ownership_bytes);
        zero(g_world_rgbkey_index_hist,sizeof(g_world_rgbkey_index_hist));
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
    g_endframe_depth++;
    g_frames++;
    if(g_log){
        if(g_endframe_depth>g_nested_endframe_max_depth)
            g_nested_endframe_max_depth=g_endframe_depth;
        if(g_endframe_depth>=2u){
            ++g_nested_endframe_events;
            /* Keep hard-to-reproduce nested callbacks observable, but
             * don't generate enormous logs in extended play sessions. */
            if(g_nested_endframe_reports<32u ||
               (g_nested_endframe_events%250u)==0u){
                char nest[410];DWORD n=0;
                DWORD mode=0xffffffffu,handle=0xffffffffu,counter=0xffffffffu;
                if(game_global_probe_supported()){
                    if(readable_span((const void*)0x00655750u,4u))
                        mode=*(const volatile DWORD*)0x00655750u;
                    if(readable_span((const void*)0x0050DCC0u,0x14u)){
                        const volatile DWORD *globals=(const volatile DWORD*)0x0050DCC0u;
                        counter=globals[0];
                        handle=globals[4];
                    }
                }
                putstr(nest,&n,"MSI_NESTED_ENDFRAME frame=");
                putunsigned(nest,&n,g_frames);
                putstr(nest,&n," depth=");
                putunsigned(nest,&n,g_endframe_depth);
                putstr(nest,&n," callback_depth=");
                putunsigned(nest,&n,g_guest_callback_depth);
                putstr(nest,&n," mode=");
                puthex(nest,&n,mode);
                putstr(nest,&n," video_handle=");
                puthex(nest,&n,handle);
                putstr(nest,&n," video_counter=");
                puthex(nest,&n,counter);
                putstr(nest,&n," total=");
                putunsigned(nest,&n,g_nested_endframe_events);
                nest[n]=0;logtext(nest);
                ++g_nested_endframe_reports;
            }
        }
    }
    /* No numbered frame snapshots: F12 is the only screenshot trigger. */
    if(g_manual_capture_active){
        char frame[100],coverage[100],atlas[100],world[100];
        if(next_capture_pair(frame,coverage,atlas,world)){
            capture_frame(frame,0u);
            capture_ownership(coverage);
            capture_atlas_provenance(atlas);
            capture_world_provenance(world);
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
    }
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
        lognum("PERSPECTIVE_UV enabled=",(unsigned)g_perspective_enabled,"triangles=",g_perspective_triangles);
        lognum("PERSPECTIVE_UV adjusted_pixels=",g_perspective_pixels,"varying_invw_world_tris=",g_world_varying_invw);
        lognum("PERSPECTIVE_UV constant_invw_world_tris=",g_world_constant_invw,"invalid_invw_world_tris=",g_world_invalid_invw);
        lognum("WORLD_BLUE_KEY samples=",g_world_blue_samples,"transparent_state_samples=",g_world_blue_eligible);
        lognum("WORLD_BLUE_KEY skipped=",g_world_blue_skipped,"enabled=",(unsigned)g_world_blue_key_enabled);
        lognum("INDEXED_KEY candidates=",g_index_key_candidates,"skipped=",g_index_key_skipped);
        lognum("INDEXED_KEY blue_candidates=",g_index_key_blue_candidates,"other_candidates=",g_index_key_other_candidates);
        lognum("INDEXED_KEY enabled=",(unsigned)g_index_key_enabled,NULL,0);
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
    g_params++;
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
        g_info.msiColor.pMem=(LPBYTE)g_color;
        g_info.msiDepth.pMem=(LPBYTE)g_depth;
        if(g_params<=25 || ((before_color!=(DWORD)g_color || before_depth!=(DWORD)g_depth) && g_params%200==0)){
            char buf[940]; DWORD n=0;
            putstr(buf,&n,"msiSetParameters NULL REMAP call=");putunsigned(buf,&n,g_params);
            putstr(buf,&n," color ");puthex(buf,&n,before_color);
            putstr(buf,&n," -> ");puthex(buf,&n,(DWORD)g_color);
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
static BOOL readable_span(const void *p, DWORD len){
    if(!p)return FALSE;
    DWORD start=(DWORD)p;
    if(start<65536u || start>0xffffffffu-len)return FALSE;
    MEMORY_BASIC_INFORMATION32 mem;
    if(VirtualQuery(p,&mem,sizeof(mem))!=sizeof(mem))return FALSE;
    if(mem.State!=MEM_COMMIT || (mem.Protect & PAGE_GUARD) ||
       (mem.Protect & 0xffu)==PAGE_NOACCESS)return FALSE;
    DWORD base=(DWORD)mem.BaseAddress;
    return start>=base && start-base<=mem.RegionSize && len<=mem.RegionSize-(start-base);
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
static float cross2(float ax,float ay,float bx,float by){return ax*by-ay*bx;}
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
    unsigned texOffset=0;
    int perspective=0;
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
        }
        if(!(a->u>=-10000.f&&a->u<=10000.f&&b->u>=-10000.f&&b->u<=10000.f&&
             c->u>=-10000.f&&c->u<=10000.f&&a->v>=-10000.f&&a->v<=10000.f&&
             b->v>=-10000.f&&b->v<=10000.f&&c->v>=-10000.f&&c->v<=10000.f)){
            g_rejected_triangles++;return;
        }
    }
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
                int preview_index1_opaque=
                    !g_zplane_draw_context && source_index_valid &&
                    source_index==1u && texW==128u && texH==128u &&
                    color==0u && g_state.msiTexture.Transparency &&
                    g_state.msiTexture.KeyingColor==0u &&
                    g_state.msiTexture.KeyingColorMask==0xffffu;
                if(g_state.msiTexture.Transparency &&
                   ((color&g_state.msiTexture.KeyingColorMask)==
                    (g_state.msiTexture.KeyingColor&g_state.msiTexture.KeyingColorMask)) &&
                   !preview_index1_opaque){
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
                if(preview_index1_opaque && g_capture_ownership &&
                   g_world_provenance)
                    g_world_provenance[(unsigned)y*(unsigned)width+
                                       (unsigned)x]=12u;
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
                    unsigned rr=(((color>>11)&31u)*mr+127u)/255u;
                    unsigned gg=(((color>>5)&63u)*mg+127u)/255u;
                    unsigned bb=((color&31u)*mb+127u)/255u;
                    unsigned reference=(rr<<11)|(gg<<5)|bb;
                    if(g_msi_dither_enabled && g_state.msiColor.Dither){
                        color=rgb565_modulate_ordered(color,mr,mg,mb,(unsigned)x,(unsigned)y);
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
            }
            dst[y*width+x]=(WORD)color;
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
    if(pixelCount>0)g_drawn_triangles++;
    g_drawn_pixels+=pixelCount;
}
static void rasterize_triangle(const T_msiVertex*a,const T_msiVertex*b,const T_msiVertex*c,BYTE opacity){
    DWORD t=GetTickCount();
    rasterize_triangle_body(a,b,c,opacity);
    g_raster_ms+=GetTickCount()-t;
}
BOOL __cdecl msiRenderTriangle(T_msiVertex*a,T_msiVertex*b,T_msiVertex*c,BYTE opacity){
    if(!g_initialized)return FALSE;
    g_triangles++;
    g_last_triangle_frame=g_frames;
    g_last_triangle_tick=GetTickCount();
    if(g_no_geometry_logged){
       lognum("GEOMETRY_RESUMED elapsed_ms=",g_last_triangle_tick-g_boot_tick,"frame=",g_frames);
       g_no_geometry_logged=0;
    }
    if(g_triangles<=18 || g_triangles%1000==0){
        char buf[940];DWORD n=0;
        putstr(buf,&n,"MSI_TRIANGLE frame=");putunsigned(buf,&n,g_frames);
        putstr(buf,&n," count=");putunsigned(buf,&n,g_triangles);
        putstr(buf,&n," opacity=");putunsigned(buf,&n,(unsigned)opacity);
        if(readable_span(a,sizeof(*a))&&readable_span(b,sizeof(*b))&&readable_span(c,sizeof(*c))){
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
    if(readable_span(a,sizeof(*a))&&readable_span(b,sizeof(*b))&&readable_span(c,sizeof(*c)))
        rasterize_triangle(a,b,c,opacity);
    else g_rejected_triangles++;
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
    g_zplanes++;
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
