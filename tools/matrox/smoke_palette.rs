// SPDX-License-Identifier: GPL-3.0-or-later
// Test the real x86 MSI95 DLL's RGB565 color-key behavior with synthetic
// indexed textures. No copyrighted game files or original Matrox driver.
use std::ffi::c_void;
use std::ptr;

#[link(name = "kernel32")]
extern "system" {
    fn LoadLibraryA(name: *const u8) -> *mut c_void;
    fn GetProcAddress(module: *mut c_void, name: *const u8) -> *mut c_void;
    fn FreeLibrary(module: *mut c_void) -> i32;
    fn VirtualAlloc(addr: *mut c_void, size: usize,
                    allocation_type: u32, protect: u32) -> *mut c_void;
    fn VirtualProtect(addr: *mut c_void, size: usize,
                      new_protect: u32, old_protect: *mut u32) -> i32;
    fn VirtualFree(addr: *mut c_void, size: usize, free_type: u32) -> i32;
}

type GuestWndProc = unsafe extern "system" fn(*mut c_void, u32, usize, isize) -> isize;
type Init = unsafe extern "C" fn(i32, i32, i32, i32, i32, GuestWndProc) -> *mut Info;
type Start = unsafe extern "C" fn(i32, f32, f32, f32, i32, f32) -> i32;
type Set = unsafe extern "C" fn(*const Params) -> i32;
type Triangle = unsafe extern "C" fn(*const Vertex, *const Vertex, *const Vertex, u8) -> i32;
type Exit = unsafe extern "C" fn() -> i32;

#[repr(C)]
struct Info {
    major: i16,
    minor: i16,
    color: ColorInfo,
    depth: DepthInfo,
    texture: TextureInfo,
}
#[repr(C)]
struct ColorInfo {
    bytes_per_pixel: i32,
    r_depth: i32,
    g_depth: i32,
    b_depth: i32,
    a_depth: i32,
    r_shift: i32,
    g_shift: i32,
    b_shift: i32,
    a_shift: i32,
    pitch: i32,
    mem: *mut u8,
    mem_size: i32,
}
#[repr(C)]
struct DepthInfo {
    bytes_per_pixel: i32,
    depth: i32,
    shift: i32,
    pitch: i32,
    mem: *mut u8,
    mem_size: i32,
}
#[repr(C)]
struct TextureInfo {
    mem: *mut u8,
    mem_size: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct Lut {
    mem: *mut u8,
    heap: *mut u8,
    cache_offset: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct TextureParams {
    enable: i32,
    width: i32,
    height: i32,
    planes: i32,
    mem: *mut u8,
    heap: *mut u8,
    cache_offset: i32,
    lut: Lut,
    clamp_u: i32,
    clamp_v: i32,
    modulate: i32,
    decal: i32,
    transparency: i32,
    key_color: u16,
    key_mask: u16,
    key_alpha: u16,
    key_alpha_mask: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct DepthParams {
    enable: i32,
    compare: u32,
    protect: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct ColorParams {
    dither: i32,
    protect: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct Params {
    texture: TextureParams,
    depth: DepthParams,
    color: ColorParams,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct Vertex {
    x: f32,
    y: f32,
    z: f32,
    inv_w: f32,
    r: f32,
    g: f32,
    b: f32,
    mr: f32,
    mg: f32,
    mb: f32,
    u: f32,
    v: f32,
}
impl Vertex {
    fn at(x: f32, y: f32) -> Self {
        Self {
            x, y, z: 0.0, inv_w: 0.25,
            r: 255.0, g: 255.0, b: 255.0,
            mr: 255.0, mg: 255.0, mb: 255.0,
            u: 0.5, v: 0.5,
        }
    }
}
unsafe extern "system" fn guest_wndproc(
    _: *mut c_void, _: u32, _: usize, _: isize
) -> isize {
    0
}

unsafe fn require<T>(module: *mut c_void, symbol: &'static [u8]) -> T {
    let raw = GetProcAddress(module, symbol.as_ptr());
    assert!(!raw.is_null(), "Missing MSI95 export {:?}", symbol);
    std::mem::transmute_copy::<*mut c_void, T>(&raw)
}

struct Case {
    label: &'static str,
    width: usize,
    planes: usize,
    index: u8,
    transparency: bool,
    expected: u16,
    synthetic_icon01: bool,
}
const GREEN: u16 = 0x07e0;
const RED: u16 = 0xf800;

fn main() {
    // The real MSI95 GDI preview may not be instantiated on a headless CI
    // worker, but its software framebuffer is always allocated by msiInit.
    std::env::set_var("MATROX_MSI_LOG", "0");
    std::env::set_var("MATROX_MSI_FPS_LIMIT", "0");
    std::env::set_var("MATROX_MSI_BORDERLESS", "0");
    // Tests must never inherit a developer's optional keying settings.
    std::env::set_var("MATROX_MSI_INDEXED_KEY", "0");
    std::env::set_var("MATROX_MSI_WORLD_BLUE_KEY", "0");
    unsafe {
        let module = LoadLibraryA(b"msi95.dll\0".as_ptr());
        assert!(!module.is_null(), "Cannot load actual MSI95 x86 DLL");
        let init: Init = require(module, b"msiInit\0");
        let start: Start = require(module, b"msiStartFrame\0");
        let set: Set = require(module, b"msiSetParameters\0");
        let render: Triangle = require(module, b"msiRenderTriangle\0");
        let exit: Exit = require(module, b"msiExit\0");

        for enabled in [false,true] {
        std::env::set_var("MATROX_MSI_ICON01_OPAQUE", if enabled {"1"}else{"0"});
        let info = init(640, 400, 16, 0, 0, guest_wndproc);
        assert!(!info.is_null(), "MSI95 failed to allocate framebuffer");
        assert_eq!((*info).color.bytes_per_pixel, 2);
        let pitch = (*info).color.pitch as usize;
        let ptr = (*info).color.mem as *mut u16;
        assert!(!ptr.is_null() && pitch >= 640 * 2);
        assert!((*info).color.mem_size >= (pitch * 400) as i32);
        let framebuffer = std::slice::from_raw_parts_mut(ptr, pitch * 400 / 2);
        let vertices = [
            Vertex::at(40.0, 40.0),
            Vertex::at(90.0, 40.0),
            Vertex::at(40.0, 90.0),
        ];

        let cases = [
            Case { label: "128 indexed-1 black becomes opaque", width: 128, planes: 8, index: 1, transparency: true, expected: 0, synthetic_icon01: false },
            Case { label: "128 indexed-241 black stays keyed", width: 128, planes: 8, index: 241, transparency: true, expected: GREEN, synthetic_icon01: false },
            Case { label: "128 indexed-0 black stays keyed", width: 128, planes: 8, index: 0, transparency: true, expected: GREEN, synthetic_icon01: false },
            Case { label: "128 indexed-5 red stays opaque", width: 128, planes: 8, index: 5, transparency: true, expected: RED, synthetic_icon01: false },
            Case { label: "64 indexed-1 black stays keyed", width: 64, planes: 8, index: 1, transparency: true, expected: GREEN, synthetic_icon01: false },
            Case { label: "128 indexed-1 without transparency draws black", width: 128, planes: 8, index: 1, transparency: false, expected: 0, synthetic_icon01: false },
            Case { label: "128 16bit black stays keyed", width: 128, planes: 16, index: 1, transparency: true, expected: GREEN, synthetic_icon01: false },
            Case { label: "synthetic icon01 source fingerprint (not original game data)", width: 128, planes: 8, index: 241, transparency: true, expected: GREEN, synthetic_icon01: true },
            Case { label: "cache-overwrite clears icon01 match", width: 128, planes: 8, index: 241, transparency: true, expected: GREEN, synthetic_icon01: false },
        ];

        for (i, case) in cases.iter().enumerate() {
            assert_ne!(start(1, 0.0, 0.0, 0.0, 0, 0.0), 0, "msiStartFrame failed");
            framebuffer.fill(GREEN);
            let count = case.width * case.width;
            let mut texture = if case.planes == 16 {
                vec![0u8; count * 2]
            } else {
                vec![case.index; count]
            };
            if case.synthetic_icon01{
                // A synthetic FNV32 *collision* with source-index counts
                // identical to icon01, but wholly different raster bytes.
                // No 1996 game bytes are committed. This exercises the real
                // DLL identity gate and proves index241 is opaque only when
                // it recognizes the intended atlas, even with cache reuse.
                texture.fill(5);
                texture[..6992].fill(241);
                texture[8256]=241; // sample position at UV=(0.5,0.5)
                texture[6995]=3;
                texture[16380..].copy_from_slice(&[248,167,175,41]);
                assert_eq!(texture.iter().filter(|&&b|b==241).count(),6993);
                assert!(!texture.contains(&0) && !texture.contains(&1));
                let mut hash:u32=2166136261;
                for &b in &texture{
                    hash=(hash^u32::from(b)).wrapping_mul(16777619);
                }
                assert_eq!(hash,0x35107cba);
            }
            let mut lut = vec![0u8; 512];
            // Both palette entries 1 and 241 intentionally resolve to
            // RGB565 0x0000. Entry 5 is opaque red.
            lut[2 * 5] = (RED & 0xff) as u8;
            lut[2 * 5 + 1] = (RED >> 8) as u8;
            let params = Params {
                texture: TextureParams {
                    enable: 1,
                    width: case.width as i32,
                    height: case.width as i32,
                    planes: case.planes as i32,
                    mem: texture.as_mut_ptr(),
                    heap: ptr::null_mut(),
                    cache_offset: 0,
                    lut: Lut {
                        mem: if case.planes == 8 { lut.as_mut_ptr() } else { ptr::null_mut() },
                        heap: ptr::null_mut(),
                        cache_offset: 65536,
                    },
                    clamp_u: 1, clamp_v: 1,
                    modulate: 0, decal: 0,
                    transparency: i32::from(case.transparency),
                    key_color: 0, key_mask: 0xffff,
                    key_alpha: 1, key_alpha_mask: 0,
                },
                depth: DepthParams { enable: 0, compare: 0, protect: 0 },
                color: ColorParams { dither: 0, protect: 0 },
            };
            assert_ne!(set(&params), 0, "msiSetParameters failed for {}", case.label);
            assert_ne!(render(&vertices[0], &vertices[1], &vertices[2], 100), 0,
                "msiRenderTriangle failed for {}", case.label);
            let pixel = framebuffer[50 * pitch / 2 + 50];
            let expected=if case.synthetic_icon01 && enabled {0}else{case.expected};
            assert_eq!(pixel, expected,
                "Experimental icon01={} case {}: {} expected RGB565 {:#06x}, got {:#06x}",
                enabled, i, case.label, expected, pixel);
            if case.label == "128 indexed-5 red stays opaque" {
                // Exercise all three pointer-validation routes in the *real*
                // x86 DLL, not an isolated mock of the helper.
                // Existing [Vertex;3] above resides within one stack region.
                // Three private Win32 allocations force per-vertex fallback.
                let mut pages=[ptr::null_mut();3];
                for j in 0..3 {
                    pages[j]=VirtualAlloc(ptr::null_mut(),4096,0x3000,0x04);
                    assert!(!pages[j].is_null(),"VirtualAlloc vertex region {}",j);
                    ptr::write(pages[j] as *mut Vertex,vertices[j]);
                }
                framebuffer.fill(GREEN);
                assert_ne!(render(pages[0] as *const Vertex,
                                  pages[1] as *const Vertex,
                                  pages[2] as *const Vertex,100),0);
                assert_eq!(framebuffer[50*pitch/2+50],RED,
                           "Distinct committed vertex regions must render identically");

                // Null, low and inaccessible/mid-record pointers are rejected
                // without dereferencing or changing the framebuffer.
                for bad in [ptr::null(), 0x10usize as *const Vertex] {
                    framebuffer.fill(GREEN);
                    assert_ne!(render(pages[0] as *const Vertex,bad,
                                      pages[2] as *const Vertex,100),0);
                    assert_eq!(framebuffer[50*pitch/2+50],GREEN,
                               "Invalid vertex must be ignored");
                }
                let guarded=VirtualAlloc(ptr::null_mut(),8192,0x3000,0x04);
                assert!(!guarded.is_null(),"VirtualAlloc guarded vertex pages");
                let mut previous_protection=0u32;
                assert_ne!(VirtualProtect((guarded as *mut u8).add(4096) as *mut c_void,
                                          4096,0x01,&mut previous_protection),0,
                           "VirtualProtect PAGE_NOACCESS");
                let crossing=(guarded as *const u8)
                    .add(4096-std::mem::size_of::<Vertex>()/2) as *const Vertex;
                framebuffer.fill(GREEN);
                assert_ne!(render(pages[0] as *const Vertex,crossing,
                                  pages[2] as *const Vertex,100),0);
                assert_eq!(framebuffer[50*pitch/2+50],GREEN,
                           "Vertex crossing PAGE_NOACCESS must be skipped");
                assert_ne!(VirtualFree(guarded,0,0x8000),0);
                for page in pages {
                    assert_ne!(VirtualFree(page,0,0x8000),0);
                }
            }
        }
        assert_ne!(exit(), 0, "msiExit failed after palette smoke");
        }
        // Real x86 MSI95 smoke: interpolate only four fully opaque texels,
        // even if the GAME sets Texture.Transparency=1 globally. Keep
        // source-key neighbors fully crisp (never blend RGB565 key-black
        // into terrain colors). Constant-invW menus remain nearest.
        // All fixtures synthetic; no original game texture bytes.
        for filter_on in [false, true] {
            std::env::set_var("MATROX_MSI_WORLD_BILINEAR", if filter_on {"1"} else {"0"});
            std::env::set_var("MATROX_MSI_ICON01_OPAQUE", "0");
            let info = init(640,400,16,0,0,guest_wndproc);
            assert!(!info.is_null(), "Cannot init world bilinear smoke");
            let pitch=(*info).color.pitch as usize;
            let framebuffer=std::slice::from_raw_parts_mut(
                (*info).color.mem as *mut u16, pitch*400/2);
            for planes in [8usize,16usize] {
                for transparency in [false,true] {
                    for varying in [false,true] {
                    for keyed_neighbor in [false,true] {
                    for small_geometry in [false,true] {
                    for uniform_color in [false,true] {
                        assert_ne!(start(1,0.0,0.0,0.0,0,0.0),0);
                        framebuffer.fill(GREEN);
                        let mut texture=vec![0u8;128*128*(planes/8)];
                        let mut lut=vec![0u8;512];
                        lut[2*2]=(RED&255) as u8;lut[2*2+1]=(RED>>8) as u8;
                        const BLUE:u16=0x001f;
                        lut[2*3]=(BLUE&255) as u8;lut[2*3+1]=(BLUE>>8) as u8;
                        for y in 0..128 {
                            for x in 0..128 {
                                let pos=y*128+x;
                                let color=if uniform_color { BLUE }
                                          else if x<64 { RED } else { BLUE };
                                if planes==8{
                                    texture[pos]=if uniform_color {3}
                                                 else if x<64 {2} else {3};
                                }else{
                                    texture[2*pos]=(color&255) as u8;
                                    texture[2*pos+1]=(color>>8) as u8;
                                }
                            }
                        }
                        if keyed_neighbor {
                            // At U=0.5 the left bilinear tap x=63
                            // becomes key-black; the selected NEAREST
                            // center texel x=64 is still opaque blue.
                            for y in [63usize,64usize] {
                                let pos=y*128+63;
                                if planes==8{texture[pos]=1;}
                                else{texture[2*pos]=0;texture[2*pos+1]=0;}
                            }
                        }
                        let params=Params{
                            texture:TextureParams{
                                enable:1,width:128,height:128,planes:planes as i32,
                                mem:texture.as_mut_ptr(),heap:ptr::null_mut(),
                                cache_offset:0,
                                lut:Lut{
                                    mem:if planes==8{lut.as_mut_ptr()}else{ptr::null_mut()},
                                    heap:ptr::null_mut(),cache_offset:65536,
                                },
                                clamp_u:1,clamp_v:1,modulate:0,decal:0,
                                transparency:i32::from(transparency),
                                key_color:0,key_mask:0xffff,
                                key_alpha:1,key_alpha_mask:0,
                            },
                            depth:DepthParams{enable:0,compare:0,protect:0},
                            color:ColorParams{dither:0,protect:0},
                        };
                        assert_ne!(set(&params),0);
                        let mut vertices=if small_geometry {
                            // Bounding box 8x8, bypass the palette-table
                            // setup: the direct RGB565 path must match.
                            [Vertex::at(48.0,48.0),Vertex::at(56.0,48.0),
                             Vertex::at(48.0,56.0)]
                        }else{
                            // Bounding box 50x50, use the palette-table
                            // optimization for 8-bit indexed textures.
                            [Vertex::at(40.0,40.0),Vertex::at(90.0,40.0),
                             Vertex::at(40.0,90.0)]
                        };
                        if varying {vertices[1].inv_w=0.5;}
                        assert_ne!(render(&vertices[0],&vertices[1],&vertices[2],100),0);
                        let actual=framebuffer[50*pitch/2+50];
                        let expected=if filter_on && varying {
                            if keyed_neighbor && transparency {
                                BLUE // color-keyed neighbor: original nearest
                            } else if keyed_neighbor {
                                0x0010 // opaque neighbor RGB565 black + blue
                            } else if uniform_color {
                                BLUE // four identical neighbors fast path
                            } else {
                                0x8010 // all four opaque: red + blue midpoint
                            }
                        }else{BLUE};
                        assert_eq!(actual,expected,
                            "WorldBilinear={} bpp={} transparency={} varying_invw={} keyed_neighbor={} small_geometry={} uniform_color={} expected {:#06x} got {:#06x}",
                            filter_on,planes,transparency,varying,keyed_neighbor,small_geometry,uniform_color,expected,actual);
                    }
                    }
                    }
                    }
                }
            }
            assert_ne!(exit(),0);
        }
        assert_ne!(FreeLibrary(module), 0, "Cannot unload MSI95 x86 DLL");
        println!("MSI95_PALETTE_SMOKE_PASS eighteen original icon01 + 128 real bilinear assertions; triangle vertex same-region, distinct-region and invalid-memory checks");
    }
}
