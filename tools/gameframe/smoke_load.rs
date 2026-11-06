// SPDX-License-Identifier: GPL-3.0-or-later
//! Never starts a game or changes the display. Checks Win32 DLL loading.
use std::ffi::{c_char,c_void};
#[link(name="kernel32")]
unsafe extern "system" {
    fn LoadLibraryA(path:*const c_char)->*mut c_void;
    fn GetProcAddress(module:*mut c_void,proc:*const c_char)->*mut c_void;
    fn FreeLibrary(module:*mut c_void)->i32;
    fn GetLastError()->u32;
}
fn main() {
    let m=unsafe{LoadLibraryA(c"glide.dll".as_ptr())};
    assert!(!m.is_null(),"GameFrame Glide load failed: {}",unsafe{GetLastError()});
    for name in [
        c"_grGlideInit@0",c"_grSstOpen@24",c"_grBufferSwap@4",
        c"_grSstQueryHardware@4",c"_grLfbGetWritePtr@4",
        c"_grLfbBegin@0",c"_grLfbEnd@0",
    ] {
        let address=unsafe{GetProcAddress(m,name.as_ptr())};
        assert!(!address.is_null(),"Missing export {:?}",name);
        println!("Found {:?}",name);
    }
    assert_ne!(unsafe{FreeLibrary(m)},0,"Unable to unload Glide DLL");
    println!("GameFrame Glide Win32 x86 LoadLibrary smoke: PASS");
}
