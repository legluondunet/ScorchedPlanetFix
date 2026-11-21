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
}
const GREEN: u16 = 0x07e0;
const RED: u16 = 0xf800;

fn main() {
    // The real MSI95 GDI preview may not be instantiated on a headless CI
    // worker, but its software framebuffer is always allocated by msiInit.
    std::env::set_var("MATROX_MSI_LOG", "0");
    std::env::set_var("MATROX_MSI_FPS_LIMIT", "0");
    std::env::set_var("MATROX_MSI_BORDERLESS", "0");
    unsafe {
        let module = LoadLibraryA(b"msi95.dll\0".as_ptr());
        assert!(!module.is_null(), "Cannot load actual MSI95 x86 DLL");
        let init: Init = require(module, b"msiInit\0");
        let start: Start = require(module, b"msiStartFrame\0");
        let set: Set = require(module, b"msiSetParameters\0");
        let render: Triangle = require(module, b"msiRenderTriangle\0");
        let exit: Exit = require(module, b"msiExit\0");

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
            Case { label: "128 indexed-1 black becomes opaque", width: 128, planes: 8, index: 1, transparency: true, expected: 0 },
            Case { label: "128 indexed-241 black stays keyed", width: 128, planes: 8, index: 241, transparency: true, expected: GREEN },
            Case { label: "128 indexed-0 black stays keyed", width: 128, planes: 8, index: 0, transparency: true, expected: GREEN },
            Case { label: "128 indexed-5 red stays opaque", width: 128, planes: 8, index: 5, transparency: true, expected: RED },
            Case { label: "64 indexed-1 black stays keyed", width: 64, planes: 8, index: 1, transparency: true, expected: GREEN },
            Case { label: "128 indexed-1 without transparency draws black", width: 128, planes: 8, index: 1, transparency: false, expected: 0 },
            Case { label: "128 16bit black stays keyed", width: 128, planes: 16, index: 1, transparency: true, expected: GREEN },
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
            assert_eq!(pixel, case.expected,
                "Case {}: {} expected RGB565 {:#06x}, got {:#06x}",
                i, case.label, case.expected, pixel);
        }
        assert_ne!(exit(), 0, "msiExit failed after palette smoke");
        assert_ne!(FreeLibrary(module), 0, "Cannot unload MSI95 x86 DLL");
        println!("MSI95_PALETTE_SMOKE_PASS seven real rasterized palette/keying cases");
    }
}
