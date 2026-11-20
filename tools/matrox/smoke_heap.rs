// SPDX-License-Identifier: GPL-3.0-or-later
// Smoke test the REAL Win32 x86 MSI95 DLL, not a copy of heap tracking logic.
// Requires Windows i686 process and msi95.dll in the working directory.
use std::ffi::c_void;

#[link(name = "kernel32")]
extern "system" {
    fn LoadLibraryA(name: *const u8) -> *mut c_void;
    fn GetProcAddress(module: *mut c_void, name: *const u8) -> *mut c_void;
    fn FreeLibrary(module: *mut c_void) -> i32;
}

type Alloc = unsafe extern "C" fn(i32) -> *mut u8;
type Free = unsafe extern "C" fn(*mut u8) -> i32;

fn main() {
    std::env::set_var("MATROX_MSI_LOG", "0");
    unsafe {
        let module = LoadLibraryA(b"msi95.dll\0".as_ptr());
        assert!(!module.is_null(), "Cannot load real MSI95 x86 DLL");
        let alloc_raw = GetProcAddress(module, b"msiAllocTextureHeap\0".as_ptr());
        let free_raw = GetProcAddress(module, b"msiFreeTextureHeap\0".as_ptr());
        assert!(!alloc_raw.is_null() && !free_raw.is_null(),
            "Required MSI95 texture heap exports not found");
        let alloc: Alloc = std::mem::transmute(alloc_raw);
        let free: Free = std::mem::transmute(free_raw);
        // A previously hardcoded g_heaps[256] returned NULL at call #257.
        // 600 simultaneous 4-page heaps cross both 256 and 512 boundaries.
        let mut allocations = Vec::new();
        for index in 0..600 {
            let p = alloc(4);
            assert!(!p.is_null(), "MSI95 refused healthy 4-page allocation #{index}");
            allocations.push(p);
        }
        // Free in a mixed order to exercise the tracking table swap-delete.
        for index in (0..600).step_by(3) {
            assert_ne!(free(allocations[index]), 0,
                "MSI95 rejected tracked allocation at index {index}");
        }
        for (index, p) in allocations.iter().enumerate() {
            if index % 3 != 0 {
                assert_ne!(free(*p), 0,
                    "MSI95 rejected tracked allocation at index {index}");
            }
        }
        // Capacity stays available and all freed entries may be reused.
        for index in 0..600 {
            let p = alloc(4);
            assert!(!p.is_null(), "MSI95 cannot reuse tracker after freeing #{index}");
            assert_ne!(free(p), 0);
        }
        assert_ne!(FreeLibrary(module), 0, "Cannot unload MSI95 DLL");
    }
    println!("MSI95_HEAP_SMOKE_PASS 600 simultaneous allocations, 1200 frees");
}
