// SPDX-License-Identifier: GPL-3.0-or-later
//! Standalone diagnostic of the *original* Matrox OEM game WinMain return.
//! Attaches as a native 32-bit Windows debugger and sets CPU hardware execution
//! breakpoints (DR0/DR1). NEVER writes code, data, IAT or disk game files.
//! Observes WinMain's MSG.wParam just before it returns to the original CRT.
//! This is intentionally NOT part of the ScorchedPlanetFix launcher/runtime.
#![cfg(target_arch = "x86")]
use std::collections::{BTreeMap, VecDeque};
use std::env;
use std::ffi::c_void;
use std::mem::{offset_of, size_of, zeroed};
use std::process;

type Handle = *mut c_void;
const CONTEXT_FLAGS: u32 = 0x0001_0013; // i386 CONTROL|INTEGER|DEBUG_REGISTERS
const EXCEPTION_DEBUG_EVENT: u32 = 1;
const CREATE_THREAD_DEBUG_EVENT: u32 = 2;
const CREATE_PROCESS_DEBUG_EVENT: u32 = 3;
const EXIT_THREAD_DEBUG_EVENT: u32 = 4;
const EXIT_PROCESS_DEBUG_EVENT: u32 = 5;
const EXCEPTION_SINGLE_STEP: u32 = 0x8000_0004;
const EXCEPTION_BREAKPOINT: u32 = 0x8000_0003;
const DBG_CONTINUE: u32 = 0x0001_0002;
const DBG_EXCEPTION_NOT_HANDLED: u32 = 0x8001_0001;
const PROCESS_VM_READ: u32 = 0x10;
const PROCESS_QUERY_INFORMATION: u32 = 0x400;
const THREAD_GET_CONTEXT: u32 = 0x0008;
const THREAD_SET_CONTEXT: u32 = 0x0010;
const THREAD_SUSPEND_RESUME: u32 = 0x0002;
const EXIT_INSN: u32 = 0x0041_6cec;
const PEEK_RETURN_INSN: u32 = 0x0041_6a79;
const QUIT_FLAG_ADDR: u32 = 0x0069_1b28;

// 32-bit Windows CONTEXT, with its documented 112-byte FLOATING_SAVE_AREA.
// These offsets are statically checked before we attach to any process.
#[repr(C)]
#[derive(Clone,Copy)]
struct X86Context {
    flags:u32, dr0:u32, dr1:u32, dr2:u32, dr3:u32, dr6:u32, dr7:u32,
    floating_save:[u8;112], gs:u32, fs:u32, es:u32, ds:u32,
    edi:u32, esi:u32, ebx:u32, edx:u32, ecx:u32, eax:u32,
    ebp:u32, eip:u32, cs:u32, eflags:u32, esp:u32, ss:u32,
    extended:[u8;512]
}
// Oversized, four-byte aligned buffer: Windows writes only the standard
// 32-bit DEBUG_EVENT. Header is 12 bytes; union begins immediately after.
#[repr(C)]
struct DebugEvent { kind:u32, pid:u32, tid:u32, payload:[u8;160] }
#[derive(Clone,Copy)]
struct WinMsg { hwnd:u32, message:u32, wparam:u32, lparam:u32, time:u32, x:i32, y:i32 }
#[derive(Clone,Copy)]
struct Seen { msg:WinMsg, tid:u32, peek_eax:u32 }

#[link(name="kernel32")]
extern "system" {
    fn DebugActiveProcess(pid:u32)->i32;
    fn DebugActiveProcessStop(pid:u32)->i32;
    fn DebugSetProcessKillOnExit(kill:i32)->i32;
    fn WaitForDebugEvent(ev:*mut DebugEvent, millis:u32)->i32;
    fn ContinueDebugEvent(pid:u32, tid:u32, status:u32)->i32;
    fn OpenProcess(access:u32, inherit:i32, pid:u32)->Handle;
    fn OpenThread(access:u32, inherit:i32, tid:u32)->Handle;
    fn GetThreadContext(thread:Handle, ctx:*mut X86Context)->i32;
    fn SetThreadContext(thread:Handle, ctx:*const X86Context)->i32;
    fn ReadProcessMemory(process:Handle, addr:*const c_void,
                         out:*mut c_void, size:usize, read:*mut usize)->i32;
    fn CloseHandle(handle:Handle)->i32;
    fn GetLastError()->u32;
}
fn read_bytes(process:Handle,addr:u32,out:&mut [u8])->bool{
    let mut count=0usize;
    unsafe { ReadProcessMemory(process,addr as usize as *const c_void,
               out.as_mut_ptr() as *mut c_void,out.len(),&mut count)!=0 && count==out.len() }
}
fn read_u32(process:Handle,addr:u32)->Option<u32>{
    let mut b=[0u8;4];
    if !read_bytes(process,addr,&mut b){return None}
    Some(u32::from_le_bytes(b))
}
fn read_msg(process:Handle,stack:u32)->Option<WinMsg>{
    let mut b=[0u8;28];
    // WinMain: lea eax,[esp+0x1c] -> PeekMessageA(MSG*,0,0,0,PM_REMOVE).
    // At the return instruction, [esp+0x24] = MSG.wParam.
    if !read_bytes(process,stack.wrapping_add(0x1c),&mut b){return None}
    let w=|o:usize| u32::from_le_bytes(b[o..o+4].try_into().unwrap());
    Some(WinMsg{hwnd:w(0),message:w(4),wparam:w(8),lparam:w(12),
                 time:w(16),x:w(20) as i32,y:w(24) as i32})
}
fn name(m:u32)->&'static str{
    match m {0x0006=>"WM_ACTIVATE",0x0007=>"WM_SETFOCUS",0x0008=>"WM_KILLFOCUS",
      0x0012=>"WM_QUIT",0x0010=>"WM_CLOSE",0x0002=>"WM_DESTROY",
      0x0100=>"WM_KEYDOWN",0x0101=>"WM_KEYUP",0x0102=>"WM_CHAR",
      0x0111=>"WM_COMMAND",0x0200=>"WM_MOUSEMOVE",0x0201=>"WM_LBUTTONDOWN",
      0x0202=>"WM_LBUTTONUP",0x0204=>"WM_RBUTTONDOWN",0x0205=>"WM_RBUTTONUP",
      0x001c=>"WM_ACTIVATEAPP",_=>"OTHER"}
}
fn context(thread:Handle)->Option<X86Context>{
    let mut c:X86Context=unsafe{zeroed()};
    c.flags=CONTEXT_FLAGS;
    if unsafe{GetThreadContext(thread,&mut c)}!=0{Some(c)}else{None}
}
fn configure_thread(tid:u32,with_peeks:bool,enable:bool)->bool{
    let handle=unsafe{OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME,0,tid)};
    if handle.is_null(){return false;}
    let success=if let Some(mut c)=context(handle){
        c.dr0=if enable{EXIT_INSN}else{0};
        c.dr1=if enable&&with_peeks{PEEK_RETURN_INSN}else{0};
        c.dr6=0;
        c.dr7=if !enable{0}else if with_peeks{0x5}else{0x1};
        unsafe{SetThreadContext(handle,&c)!=0}
    }else{false};
    unsafe{CloseHandle(handle);}
    success
}
fn signatures(process:Handle)->bool{
    // Exact 1996 OEM WinMain assembly, from user's hash-verified CD:
    // test eax,eax after PeekMessageA; quit-flag read; mov eax,[esp+24].
    for (addr,want) in [
       (0x0041_6a79u32,&[0x85,0xc0,0x74,0x1d][..]),
       (0x0041_6cb6u32,&[0xa1,0x28,0x1b,0x69,0x00][..]),
       (0x0041_6cecu32,&[0x8b,0x44,0x24,0x24][..]),
    ]{
        let mut b=vec![0u8;want.len()];
        if !read_bytes(process,addr,&mut b)||b!=want{return false;}
    }
    true
}
fn dump(msg:WinMsg){
    println!("MSG message=0x{:04x} {} hwnd=0x{:08x} wParam=0x{:08x} ({}) lParam=0x{:08x} time={} pt=({}, {})",
       msg.message,name(msg.message),msg.hwnd,msg.wparam,msg.wparam,
       msg.lparam,msg.time,msg.x,msg.y);
}
fn main(){
    if size_of::<X86Context>()!=716||offset_of!(X86Context,eip)!=184||
       offset_of!(X86Context,esp)!=196||size_of::<DebugEvent>()<96 {
       eprintln!("Unsupported x86 Windows debug structures; refusing to attach.");
       process::exit(2);
    }
    let args:Vec<_>=env::args().skip(1).collect();
    if args.len()==1&&args[0]=="--self-test" {
        let test=WinMsg{hwnd:0,message:0x201,wparam:1,lparam:0,time:0,x:0,y:0};
        assert_eq!(name(test.message),"WM_LBUTTONDOWN");
        assert_eq!(test.wparam,1);
        println!("MATROX_EXIT_TRACE_LAYOUT_OK context=716 EIP=184 ESP=196");
        return;
    }
    if !(args.len()==1||args.len()==2) {
        eprintln!("Usage: MatroxExitTrace.exe <game_pid> [--messages]");
        eprintln!("Start the installed Matrox game normally; while it is in the menu,");
        eprintln!("run this tracer in a second terminal, then quit via the game menu.");
        eprintln!("No original executable or game memory is patched.");
        process::exit(2);
    }
    let with_peeks=args.len()==2&&args[1]=="--messages";
    if args.len()==2&&!with_peeks{eprintln!("Unknown option");process::exit(2);}
    let pid:u32=match args[0].parse(){Ok(p) if p!=0=>p,_=>{eprintln!("Invalid game PID");process::exit(2)}};
    let proc_handle=unsafe{OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,0,pid)};
    if proc_handle.is_null() {
        eprintln!("Cannot open PID {} (Win32 error {})",pid,unsafe{GetLastError()});
        process::exit(1);
    }
    if !signatures(proc_handle){
        eprintln!("REFUSED: PID {} does not have the exact recognized original Matrox WinMain code.",pid);
        unsafe{CloseHandle(proc_handle);}
        process::exit(1);
    }
    if unsafe{DebugActiveProcess(pid)}==0 {
        eprintln!("Cannot attach Windows debugger (error {})",unsafe{GetLastError()});
        unsafe{CloseHandle(proc_handle);}
        process::exit(1);
    }
    if unsafe{DebugSetProcessKillOnExit(0)}==0 {
        eprintln!("Cannot guarantee game survival if tracer exits; detaching.");
        unsafe{DebugActiveProcessStop(pid);CloseHandle(proc_handle);}
        process::exit(1);
    }
    println!("ATTACHED pid={} mode={} OEM signature verified, CPU hardware breakpoints only.",
         pid,if with_peeks{"trace successful PeekMessageA results"}else{"WinMain return only"});
    let mut threads=BTreeMap::<u32,bool>::new();
    let mut last=VecDeque::<Seen>::new();
    let mut captured=false;
    let mut ended=false;
    loop {
        let mut ev:DebugEvent=unsafe{zeroed()};
        if unsafe{WaitForDebugEvent(&mut ev,30000)}==0{
            eprintln!("WaitForDebugEvent failed/timeout (error {})",unsafe{GetLastError()});
            // Avoid abandoning active hardware breakpoints on an unknown state.
            continue;
        }
        let mut disposition=DBG_CONTINUE;
        match ev.kind {
            CREATE_THREAD_DEBUG_EVENT|CREATE_PROCESS_DEBUG_EVENT=>{
                let ok=configure_thread(ev.tid,with_peeks,true);
                threads.insert(ev.tid,ok);
                if !ok{eprintln!("WARN: unable to set hardware breakpoints for thread {}.",ev.tid);}
            }
            EXIT_THREAD_DEBUG_EVENT=>{threads.remove(&ev.tid);}
            EXIT_PROCESS_DEBUG_EVENT=>{
                let code=u32::from_le_bytes(ev.payload[0..4].try_into().unwrap());
                println!("PROCESS_EXIT raw_code={} (0x{:08x}) captured_winmain={}",code,code,captured);
                ended=true;
            }
            EXCEPTION_DEBUG_EVENT=>{
                let exception=u32::from_le_bytes(ev.payload[0..4].try_into().unwrap());
                if exception==EXCEPTION_SINGLE_STEP {
                    let h=unsafe{OpenThread(THREAD_GET_CONTEXT,0,ev.tid)};
                    let c=if h.is_null(){None}else{context(h)};
                    if !h.is_null(){unsafe{CloseHandle(h);}}
                    match c {
                      Some(c) if c.eip==PEEK_RETURN_INSN && with_peeks=>{
                        if c.eax!=0 {
                            if let Some(m)=read_msg(proc_handle,c.esp){
                                if last.len()==12{last.pop_front();}
                                last.push_back(Seen{msg:m,tid:ev.tid,peek_eax:c.eax});
                            }
                        }
                      }
                      Some(c) if c.eip==EXIT_INSN=>{
                        captured=true;
                        println!("WINMAIN_RETURN pid={} tid={} at 0x{:08x} current quit_flag={:?}",
                            pid,ev.tid,c.eip,read_u32(proc_handle,QUIT_FLAG_ADDR));
                        if let Some(m)=read_msg(proc_handle,c.esp){
                            dump(m);
                            println!("PREDICTED_CRT_EXIT_CODE={} source=WinMain_MSG_wParam_at_ESP_plus_0x24",
                                     m.wparam);
                            if with_peeks {
                                println!("RECENT_SUCCESSFUL_PEEK_MESSAGES (max 12):");
                                for item in &last{
                                    print!("  tid={} peek_ret={} ",item.tid,item.peek_eax);
                                    dump(item.msg);
                                }
                            }
                        }else{eprintln!("FAILED: cannot read WinMain MSG local stack.");}
                        // Remove CPU breakpoints in *all* known game threads
                        // while this debug event has stopped the process.
                        for (&tid,_) in &threads{
                            if !configure_thread(tid,with_peeks,false){
                                eprintln!("WARN: failed to clear DR0/DR1 on thread {}",tid);
                            }
                        }
                    }
                      _=>{disposition=DBG_EXCEPTION_NOT_HANDLED;}
                    }
                }else if exception==EXCEPTION_BREAKPOINT{
                    disposition=DBG_CONTINUE; // initial attach break
                }else{
                    println!("GAME_EXCEPTION code=0x{:08x}; forwarding to game/Wine unchanged",exception);
                    disposition=DBG_EXCEPTION_NOT_HANDLED;
                }
            }
            _=>{}
        }
        unsafe{ContinueDebugEvent(ev.pid,ev.tid,disposition);}
        if ended||captured{break;}
    }
    if !ended {
        if unsafe{DebugActiveProcessStop(pid)}==0{
            eprintln!("WARN: debugger detach failed; game process may have already ended.");
        }else{println!("DETACHED (game continues with its original code and exit path).");}
    }
    unsafe{CloseHandle(proc_handle);}
}
