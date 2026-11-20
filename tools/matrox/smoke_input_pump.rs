// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the REAL Windows x86 MSI95 renderer, not a reimplementation.
// A synthetic OEM callback renders a nested frame while more keys are queued.
// The Win32 pump must deliver all queued messages, in order, without
// recursively entering the OEM callback during the nested frame.
use std::ffi::c_void;
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering};

#[link(name = "kernel32")]
extern "system" {
    fn LoadLibraryA(name: *const u8) -> *mut c_void;
    fn GetProcAddress(module: *mut c_void, name: *const u8) -> *mut c_void;
    fn FreeLibrary(module: *mut c_void) -> i32;
}
#[link(name = "user32")]
extern "system" {
    fn FindWindowA(class: *const u8, title: *const u8) -> *mut c_void;
    fn PostMessageA(hwnd: *mut c_void, msg: u32, wparam: usize, lparam: isize) -> i32;
}

const WM_KEYDOWN: u32 = 0x0100;
const FIRST_KEY: usize = 0x41; // A
const KEY_COUNT: usize = 25; // A through Y

type GuestWndProc = unsafe extern "system" fn(*mut c_void, u32, usize, isize) -> isize;
type MsiInit = unsafe extern "C" fn(i32, i32, i32, i32, i32, GuestWndProc) -> *mut c_void;
type MsiEndFrame = unsafe extern "C" fn(i32, i32, i32) -> i32;
type MsiExit = unsafe extern "C" fn() -> i32;

static CALLBACK_DEPTH: AtomicUsize = AtomicUsize::new(0);
static MAX_CALLBACK_DEPTH: AtomicUsize = AtomicUsize::new(0);
static KEY_EVENTS: AtomicUsize = AtomicUsize::new(0);
static CALLBACK_ERRORS: AtomicUsize = AtomicUsize::new(0);
// Used only on the same Win32 UI thread that owns the preview window.
static mut NESTED_END_FRAME: Option<MsiEndFrame> = None;

unsafe extern "system" fn guest_wndproc(
    hwnd: *mut c_void, msg: u32, key: usize, _: isize
) -> isize {
    if msg != WM_KEYDOWN || !(FIRST_KEY..FIRST_KEY + KEY_COUNT).contains(&key) {
        return 0;
    }
    let depth = CALLBACK_DEPTH.fetch_add(1, Ordering::SeqCst) + 1;
    MAX_CALLBACK_DEPTH.fetch_max(depth, Ordering::SeqCst);
    let index = KEY_EVENTS.fetch_add(1, Ordering::SeqCst);
    if key != FIRST_KEY + index {
        CALLBACK_ERRORS.fetch_add(1, Ordering::SeqCst);
    }
    if index == 0 {
        // The first callback schedules a burst and immediately renders
        // additional MSI95 frames before it returns to the guest game.
        for offset in 1..KEY_COUNT {
            if PostMessageA(hwnd, WM_KEYDOWN, FIRST_KEY + offset, 0) == 0 {
                CALLBACK_ERRORS.fetch_add(1, Ordering::SeqCst);
            }
        }
        if let Some(end_frame) = NESTED_END_FRAME {
            if end_frame(0, 0, 0) == 0 || end_frame(0, 0, 0) == 0 {
                CALLBACK_ERRORS.fetch_add(1, Ordering::SeqCst);
            }
        } else {
            CALLBACK_ERRORS.fetch_add(1, Ordering::SeqCst);
        }
    }
    CALLBACK_DEPTH.fetch_sub(1, Ordering::SeqCst);
    0
}

fn main() {
    std::env::set_var("MATROX_MSI_LOG", "0");
    std::env::set_var("MATROX_MSI_FPS_LIMIT", "0");
    std::env::set_var("MATROX_MSI_BORDERLESS", "0");
    unsafe {
        let module = LoadLibraryA(b"msi95.dll\0".as_ptr());
        assert!(!module.is_null(), "Cannot load actual MSI95 x86 DLL");
        let init_raw = GetProcAddress(module, b"msiInit\0".as_ptr());
        let end_raw = GetProcAddress(module, b"msiEndFrame\0".as_ptr());
        let exit_raw = GetProcAddress(module, b"msiExit\0".as_ptr());
        assert!(!init_raw.is_null() && !end_raw.is_null() && !exit_raw.is_null(),
            "MSI95 x86 frame/initialization exports missing");
        let init: MsiInit = std::mem::transmute(init_raw);
        let end_frame: MsiEndFrame = std::mem::transmute(end_raw);
        let exit: MsiExit = std::mem::transmute(exit_raw);
        NESTED_END_FRAME = Some(end_frame);
        let info = init(640, 400, 16, 0, 0, guest_wndproc);
        assert!(!info.is_null(), "msiInit framebuffer setup failed");
        let hwnd = FindWindowA(b"MSI95_Class\0".as_ptr(), ptr::null());
        assert!(!hwnd.is_null(), "MSI95 preview HWND not created");
        assert_ne!(PostMessageA(hwnd, WM_KEYDOWN, FIRST_KEY, 0), 0,
            "Could not enqueue initial input");
        // Outer frame handles A; its guest callback queues B..Y and
        // re-enters msiEndFrame. Nested frame must not dispatch B..Y.
        assert_ne!(end_frame(0, 0, 0), 0, "Outermost msiEndFrame failed");
        for _ in 0..4 {
            if KEY_EVENTS.load(Ordering::SeqCst) == KEY_COUNT { break; }
            assert_ne!(end_frame(0, 0, 0), 0, "Follow-up frame failed");
        }
        let received = KEY_EVENTS.load(Ordering::SeqCst);
        let max_depth = MAX_CALLBACK_DEPTH.load(Ordering::SeqCst);
        assert_eq!(received, KEY_COUNT, "Input burst lost a message");
        assert_eq!(max_depth, 1, "OEM callback recursively reentered");
        assert_eq!(CALLBACK_ERRORS.load(Ordering::SeqCst), 0,
            "Input order changed or nested renderer failed");
        assert_eq!(CALLBACK_DEPTH.load(Ordering::SeqCst), 0,
            "Guest callback did not finish");
        assert_ne!(exit(), 0, "msiExit failed");
        assert_ne!(FreeLibrary(module), 0, "Cannot unload MSI95 DLL");
        println!("MSI95_INPUT_PUMP_SMOKE_PASS {received} ordered keys, max_depth={max_depth}");
    }
}
