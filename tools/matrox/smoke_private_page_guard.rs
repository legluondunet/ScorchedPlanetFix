// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated Windows x86 proof of same-thread read/write fault retirement.
// No game/DLL loaded, no Vulkan call, no allocation or logging in handler.
// This is NOT a cross-thread or general guest ownership guarantee.
use std::{ffi::c_void, ptr, sync::atomic::{AtomicUsize, Ordering::SeqCst}};

#[repr(C)]
struct Record {
    code:u32, flags:u32, next:*mut Record, address:*mut c_void,
    count:u32, info:[usize;15],
}
#[repr(C)]
struct ExceptionPointers {record:*mut Record, context:*mut c_void}
#[link(name="kernel32")]
extern "system" {
    fn VirtualAlloc(addr:*mut c_void,size:usize,kind:u32,protect:u32)->*mut c_void;
    fn VirtualFree(addr:*mut c_void,size:usize,kind:u32)->i32;
    fn VirtualProtect(addr:*mut c_void,size:usize,protect:u32,old:*mut u32)->i32;
    fn GetCurrentThreadId()->u32;
    fn AddVectoredExceptionHandler(first:u32,handler:unsafe extern "system" fn(*mut ExceptionPointers)->i32)->*mut c_void;
    fn RemoveVectoredExceptionHandler(handle:*mut c_void)->u32;
}
const NOACCESS:u32=1;
const READWRITE:u32=4;
const SIZE:usize=8192;
static BASE:AtomicUsize=AtomicUsize::new(0);
static OWNER:AtomicUsize=AtomicUsize::new(0);
static ARMED:AtomicUsize=AtomicUsize::new(0);
static READS:AtomicUsize=AtomicUsize::new(0);
static WRITES:AtomicUsize=AtomicUsize::new(0);
unsafe extern "system" fn handler(p:*mut ExceptionPointers)->i32 {
    if p.is_null()||(*p).record.is_null(){return 0;}
    let r=&*(*p).record;
    let base=BASE.load(SeqCst);
    if r.code!=0xc0000005||r.count<2||!matches!(r.info[0],0|1)||
       base==0||r.info[1].wrapping_sub(base)>=SIZE||
       GetCurrentThreadId() as usize!=OWNER.load(SeqCst)||
       ARMED.compare_exchange(1,2,SeqCst,SeqCst).is_err(){return 0;}
    let mut old=0;
    if VirtualProtect(base as *mut c_void,SIZE,READWRITE,&mut old)==0 {
        ARMED.store(0,SeqCst);return 0;
    }
    // Synthetic pending write becomes visible BEFORE resuming the faulting
    // instruction. No external library or guest memory is accessed.
    ptr::write_volatile((base+4096) as *mut u32,0x12345678);
    if r.info[0]==0{READS.fetch_add(1,SeqCst);}else{WRITES.fetch_add(1,SeqCst);}
    ARMED.store(0,SeqCst);
    -1
}
unsafe fn arm(base:*mut c_void){
    assert_eq!(ARMED.load(SeqCst),0);
    ARMED.store(1,SeqCst);
    let mut old=0;
    assert_ne!(VirtualProtect(base,SIZE,NOACCESS,&mut old),0);
    assert_eq!(old,READWRITE);
}
fn main(){
    unsafe {
        let base=VirtualAlloc(ptr::null_mut(),SIZE,0x3000,READWRITE);
        assert!(!base.is_null());
        BASE.store(base as usize,SeqCst);
        OWNER.store(GetCurrentThreadId() as usize,SeqCst);
        let veh=AddVectoredExceptionHandler(1,handler);
        assert!(!veh.is_null());
        // Retained alias to second page: direct read must see pending data.
        let alias=(base as usize+4096) as *mut u32;
        ptr::write_volatile(alias,0);
        arm(base);
        assert_eq!(ptr::read_volatile(alias),0x12345678);
        assert_eq!(READS.load(SeqCst),1);
        // Direct write must execute AFTER pending work and win over it.
        arm(base);
        ptr::write_volatile(alias,0x89abcdef);
        assert_eq!(ptr::read_volatile(alias),0x89abcdef);
        assert_eq!(WRITES.load(SeqCst),1);
        assert_eq!(ARMED.load(SeqCst),0);
        // Explicit API boundary may restore without a fault.
        arm(base);
        let mut old=0;
        assert_ne!(VirtualProtect(base,SIZE,READWRITE,&mut old),0);
        assert_eq!(old,NOACCESS);
        ARMED.store(0,SeqCst);
        assert_eq!(ptr::read_volatile(alias),0x89abcdef);
        // Unrelated faults, execute attempts and foreign-thread identity
        // must not be consumed. Exercise classifier with synthetic records.
        let mut r=Record{code:0xc0000005,flags:0,next:ptr::null_mut(),address:ptr::null_mut(),count:2,info:[0;15]};
        let mut p=ExceptionPointers{record:&mut r,context:ptr::null_mut()};
        r.info[1]=base as usize+SIZE;
        assert_eq!(handler(&mut p),0);
        r.info[1]=alias as usize;r.info[0]=8;
        assert_eq!(handler(&mut p),0);
        r.info[0]=0;OWNER.store(0,SeqCst);
        assert_eq!(handler(&mut p),0);
        assert_ne!(RemoveVectoredExceptionHandler(veh),0);
        BASE.store(0,SeqCst);
        assert_ne!(VirtualFree(base,0,0x8000),0);
    }
    println!("MSI95_PRIVATE_PAGE_GUARD_PASS read=1 write=1 explicit=1");
}
