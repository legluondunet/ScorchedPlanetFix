// SPDX-License-Identifier: GPL-3.0-or-later
//! Equal-workload x86 MSI95 UV benchmark. No original game assets.
//! Run independently against optimized and benchmark-reference DLLs.
//! Only rendering calls are timed. Full framebuffer hashes are captured
//! after each batch, outside the timer, to test parity beyond one final pixel.
use std::ffi::{c_void, CString};
use std::ptr;

#[link(name = "kernel32")]
extern "system" {
    fn LoadLibraryA(name: *const u8) -> *mut c_void;
    fn GetProcAddress(module: *mut c_void, name: *const u8) -> *mut c_void;
    fn FreeLibrary(module: *mut c_void) -> i32;
    fn QueryPerformanceCounter(value: *mut i64) -> i32;
    fn QueryPerformanceFrequency(value: *mut i64) -> i32;
}
type GuestWndProc = unsafe extern "system" fn(*mut c_void, u32, usize, isize) -> isize;
type Init = unsafe extern "C" fn(i32, i32, i32, i32, i32, GuestWndProc) -> *mut Info;
type Start = unsafe extern "C" fn(i32, f32, f32, f32, i32, f32) -> i32;
type Set = unsafe extern "C" fn(*const Params) -> i32;
type Triangle = unsafe extern "C" fn(*const Vertex, *const Vertex, *const Vertex, u8) -> i32;
type Exit = unsafe extern "C" fn() -> i32;

#[repr(C)]
struct Info { major: i16, minor: i16, color: ColorInfo, depth: DepthInfo, texture: TextureInfo }
#[repr(C)]
struct ColorInfo {
    bytes_per_pixel:i32, r_depth:i32, g_depth:i32, b_depth:i32, a_depth:i32,
    r_shift:i32, g_shift:i32, b_shift:i32, a_shift:i32,
    pitch:i32, mem:*mut u8, mem_size:i32,
}
#[repr(C)]
struct DepthInfo { bytes_per_pixel:i32, depth:i32, shift:i32, pitch:i32, mem:*mut u8, mem_size:i32 }
#[repr(C)]
struct TextureInfo { mem:*mut u8, mem_size:i32 }
#[repr(C)]
#[derive(Clone, Copy)]
struct Lut { mem:*mut u8, heap:*mut u8, cache_offset:i32 }
#[repr(C)]
#[derive(Clone, Copy)]
struct TextureParams {
    enable:i32,width:i32,height:i32,planes:i32,
    mem:*mut u8,heap:*mut u8,cache_offset:i32,lut:Lut,
    clamp_u:i32,clamp_v:i32,modulate:i32,decal:i32,transparency:i32,
    key_color:u16,key_mask:u16,key_alpha:u16,key_alpha_mask:u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct DepthParams { enable:i32,compare:u32,protect:i32 }
#[repr(C)]
#[derive(Clone, Copy)]
struct ColorParams { dither:i32,protect:i32 }
#[repr(C)]
#[derive(Clone, Copy)]
struct Params { texture:TextureParams, depth:DepthParams, color:ColorParams }
#[repr(C)]
#[derive(Clone, Copy)]
struct Vertex {
    x:f32,y:f32,z:f32,inv_w:f32,r:f32,g:f32,b:f32,
    mr:f32,mg:f32,mb:f32,u:f32,v:f32,
}
impl Vertex {
    fn at(x:f32,y:f32,u:f32,v:f32)->Self {
        Self{x,y,z:0.,inv_w:0.25,r:255.,g:255.,b:255.,
             mr:255.,mg:255.,mb:255.,u,v}
    }
}
unsafe extern "system" fn guest_wndproc(_: *mut c_void,_:u32,_:usize,_:isize)->isize {0}
unsafe fn require<T>(dll:*mut c_void,sym:&'static [u8])->T {
    let f=GetProcAddress(dll,sym.as_ptr());
    assert!(!f.is_null(),"DLL export missing: {:?}",sym);
    std::mem::transmute_copy::<*mut c_void,T>(&f)
}
unsafe fn qpc()->i64 {
    let mut t=0i64;
    assert_ne!(QueryPerformanceCounter(&mut t),0);
    t
}
const BATCHES:usize=24;
const TRIANGLES_PER_BATCH:usize=70;
// Each profile performs exactly 1680 identical triangles in both DLLs.
fn main(){
    let name=std::env::args().nth(1).expect("Usage: MatroxUvBench.exe DLL_PATH");
    std::env::set_var("MATROX_MSI_LOG","0");
    std::env::set_var("MATROX_MSI_FPS_LIMIT","0");
    std::env::set_var("MATROX_MSI_BORDERLESS","0");
    std::env::set_var("MATROX_MSI_WORLD_BILINEAR","0");
    std::env::set_var("MATROX_MSI_PERSPECTIVE","0");
    std::env::set_var("MATROX_MSI_MODULATE","0");
    std::env::set_var("MATROX_MSI_DITHER","0");
    std::env::set_var("MATROX_MSI_INDEXED_KEY","0");
    std::env::set_var("MATROX_MSI_SHADOW_ONCE","0");
    unsafe {
        let c_name=CString::new(name.as_bytes()).unwrap();
        let dll=LoadLibraryA(c_name.as_ptr() as *const u8);
        assert!(!dll.is_null(),"Cannot load {}",name);
        let init:Init=require(dll,b"msiInit\0");
        let start:Start=require(dll,b"msiStartFrame\0");
        let set:Set=require(dll,b"msiSetParameters\0");
        let render:Triangle=require(dll,b"msiRenderTriangle\0");
        let exit:Exit=require(dll,b"msiExit\0");
        let info=init(640,400,16,0,0,guest_wndproc);
        assert!(!info.is_null(),"MSI95 init failed");
        let pitch=(*info).color.pitch as usize/2;
        let framebuffer=std::slice::from_raw_parts_mut(
            (*info).color.mem as *mut u16,(*info).color.mem_size as usize/2);
        assert!(pitch>=640 && framebuffer.len()>=pitch*400);
        let mut freq=0i64;
        assert_ne!(QueryPerformanceFrequency(&mut freq),0);
        assert!(freq>0);
        // Synthetic CLUT texture; no original game data.
        let mut texture=vec![0u8;128*128];
        let mut lut=vec![0u8;512];
        for y in 0..128usize {
            for x in 0..128usize {
                texture[y*128+x]=1+(((x*7+y*11+(x*y)%17)%63) as u8);
            }
        }
        for i in 1..64usize {
            let color:u16=((((i*5)%32) as u16)<<11) |
                          ((((i*3)%64) as u16)<<5) | (((i*13)%32) as u16);
            lut[2*i]=(color&255) as u8;
            lut[2*i+1]=(color>>8) as u8;
        }
        let params=Params{
            texture:TextureParams{
                enable:1,width:128,height:128,planes:8,
                mem:texture.as_mut_ptr(),heap:ptr::null_mut(),
                cache_offset:0,
                lut:Lut{mem:lut.as_mut_ptr(),heap:ptr::null_mut(),cache_offset:65536},
                clamp_u:0,clamp_v:0,modulate:0,decal:0,transparency:0,
                key_color:0,key_mask:0xffff,key_alpha:0,key_alpha_mask:0,
            },
            depth:DepthParams{enable:0,compare:0,protect:0},
            color:ColorParams{dither:0,protect:0},
        };
        assert_ne!(set(&params),0,"Cannot bind synthetic texture");
        for profile in 0..2 {
            assert_ne!(start(1,0.,0.,0.,0,0.),0);
            let mut elapsed_ticks=0i64;
            let mut hash:u64=0xcbf29ce484222325;
            for batch in 0..BATCHES {
                framebuffer.fill(0x1357);
                let start_ticks=qpc();
                for i in 0..TRIANGLES_PER_BATCH {
                    let k=batch*TRIANGLES_PER_BATCH+i;
                    let dx=((k*13)%85) as f32;
                    let dy=((k*17)%48) as f32;
                    let vertices=if profile==0 {
                        // In-range interpolated U/V; the experimental branch
                        // is eligible for almost every fragment.
                        [
                            Vertex::at(72.+dx,38.+dy,0.08,0.07),
                            Vertex::at(298.+dx,38.+dy,0.91,0.11),
                            Vertex::at(72.+dx,206.+dy,0.18,0.93),
                        ]
                    }else{
                        // Repeating tiled textures: original floor fallback
                        // remains necessary for negative/>=1 fragments.
                        [
                            Vertex::at(72.+dx,38.+dy,-1.38,-0.43),
                            Vertex::at(298.+dx,38.+dy,2.71,0.82),
                            Vertex::at(72.+dx,206.+dy,0.47,3.23),
                        ]
                    };
                    assert_ne!(render(&vertices[0],&vertices[1],&vertices[2],100),0);
                }
                elapsed_ticks+=qpc()-start_ticks;
                // Reading every frame's complete framebuffer *outside*
                // measured time detects transient mismatches/overdraw too.
                for &pixel in framebuffer.iter() {
                    hash=(hash ^ u64::from(pixel)).wrapping_mul(0x100000001b3);
                }
            }
            let us=elapsed_ticks.saturating_mul(1_000_000)/freq;
            let label=if profile==0{"inrange"}else{"tiled"};
            println!("BENCH_UV profile={} render_us={} checksum={:016x} triangles={}",
                     label,us,hash,BATCHES*TRIANGLES_PER_BATCH);
        }
        assert_ne!(exit(),0,"MSI95 exit failed");
        assert_ne!(FreeLibrary(dll),0,"MSI95 unload failed");
    }
}