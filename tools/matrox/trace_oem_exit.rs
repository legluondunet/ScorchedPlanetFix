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
const TH32CS_SNAPPROCESS: u32 = 0x0000_0002;
const THREAD_GET_CONTEXT: u32 = 0x0008;
const THREAD_SET_CONTEXT: u32 = 0x0010;
const THREAD_SUSPEND_RESUME: u32 = 0x0002;
const EXIT_INSN: u32 = 0x0041_6cec;
const PEEK_RETURN_INSN: u32 = 0x0041_6a79;
const CRT_WINMAIN_RESULT_INSN: u32 = 0x004a_7130; // push eax after original call WinMain
const CRT_EXITPROCESS_ARGUMENT_INSN: u32 = 0x004a_c416; // push esi just before imported ExitProcess
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
// PROCESSENTRY32W, fixed 556-byte x86 layout. Windows process IDs, not Unix PIDs.
#[repr(C)]
struct ProcessEntry32W {
    size:u32, usage:u32, pid:u32, default_heap_id:u32,
    module_id:u32, threads:u32, parent_pid:u32, base_priority:i32,
    flags:u32, image_name:[u16;260],
}
#[derive(Clone,Copy)]
struct WinMsg { hwnd:u32, message:u32, wparam:u32, lparam:u32, time:u32, x:i32, y:i32 }
#[derive(Clone,Copy)]
struct Seen { msg:WinMsg, tid:u32, peek_eax:u32 }

#[link(name="kernel32")]
extern "system" {
    fn CreateToolhelp32Snapshot(flags:u32, pid:u32)->Handle;
    fn Process32FirstW(snapshot:Handle, entry:*mut ProcessEntry32W)->i32;
    fn Process32NextW(snapshot:Handle, entry:*mut ProcessEntry32W)->i32;
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
#[derive(Clone,Copy,PartialEq,Eq)]
enum TraceStage { WinMain, CrtExit, CrtExitArgument, ProcessExit }
fn configure_thread(tid:u32,with_peeks:bool,stage:TraceStage)->bool{
    let handle=unsafe{OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME,0,tid)};
    if handle.is_null(){return false;}
    let success=if let Some(mut c)=context(handle){
        let (bp0,bp1)=match stage {
            TraceStage::WinMain=>(EXIT_INSN,if with_peeks{PEEK_RETURN_INSN}else{0}),
            TraceStage::CrtExit=>(CRT_WINMAIN_RESULT_INSN,CRT_EXITPROCESS_ARGUMENT_INSN),
            TraceStage::CrtExitArgument=>(CRT_EXITPROCESS_ARGUMENT_INSN,0),
            TraceStage::ProcessExit=>(0,0)
        };
        c.dr0=bp0;
        c.dr1=bp1;
        c.dr6=0;
        c.dr7=if bp1!=0 {0x5} else if bp0!=0 {0x1} else {0};
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
       (CRT_WINMAIN_RESULT_INSN,&[0x50,0xe8,0xfa,0x51,0x00,0x00][..]),
       (CRT_EXITPROCESS_ARGUMENT_INSN,&[0x56,0xff,0x15,0x80,0x53,0x6a,0x00][..]),
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
fn find_oem_games()->Vec<u32>{
    let snapshot=unsafe{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};
    if snapshot.is_null() || snapshot as isize == -1 {
        eprintln!("Cannot enumerate Windows processes (Win32 error {})", unsafe{GetLastError()});
        return Vec::new();
    }
    let mut entry:ProcessEntry32W=unsafe{zeroed()};
    entry.size=size_of::<ProcessEntry32W>() as u32;
    let mut ids=Vec::new();
    let mut ok=unsafe{Process32FirstW(snapshot,&mut entry)};
    while ok!=0 {
        let end=entry.image_name.iter().position(|x|*x==0).unwrap_or(entry.image_name.len());
        let name=String::from_utf16_lossy(&entry.image_name[..end]);
        if name.eq_ignore_ascii_case("splanetw.exe") {
            // Do not attach to another similarly-named app; game code signature must match.
            let handle=unsafe{OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,0,entry.pid)};
            if !handle.is_null(){
                if signatures(handle){ids.push(entry.pid);}
                unsafe{CloseHandle(handle);}
            }
        }
        ok=unsafe{Process32NextW(snapshot,&mut entry)};
    }
    unsafe{CloseHandle(snapshot);}
    ids
}
fn main(){
    if size_of::<X86Context>()!=716||offset_of!(X86Context,eip)!=184||
       offset_of!(X86Context,esp)!=196||size_of::<DebugEvent>()<96 ||
       size_of::<ProcessEntry32W>()!=556 {
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
    if args.len()>2 || (args.len()==2 && args[1]!="--messages") {
        eprintln!("Usage: MatroxExitTrace.exe [Windows_game_PID] [--messages]");
        eprintln!("Start OEM Matrox game first; no PID needed if one matching game is running.");
        eprintln!("With Wine use a Windows PID, NOT a Linux/Unix PID.");
        process::exit(2);
    }
    let with_peeks=args.iter().any(|a|a=="--messages");
    let supplied=if args.first().map(|a|a.as_str())!=Some("--messages") && !args.is_empty(){
        match args[0].parse::<u32>(){Ok(p) if p!=0=>Some(p),_=>{
            eprintln!("Invalid Windows PID: {}",args[0]);process::exit(2)
        }}
    }else{None};
    let games=find_oem_games();
    let pid=match supplied {
        Some(id) if games.contains(&id)=>id,
        Some(id) if games.len()==1=>{
            eprintln!("PID {} is not an active OEM Matrox Windows PID. Using uniquely detected Windows PID {}.",id,games[0]);
            games[0]
        },
        Some(id)=>{
            eprintln!("PID {} is not an active OEM Matrox Windows PID.",id);
            eprintln!("Matching OEM Matrox Windows PIDs: {:?}. Under Wine, host Linux PIDs differ.",games);
            process::exit(1)
        },
        None if games.len()==1=>{
            println!("AUTO_DETECTED Matrox OEM Windows PID={}",games[0]);games[0]
        },
        None=>{
            eprintln!("Expected exactly one running original Matrox OEM game; matches: {:?}.",games);
            eprintln!("Start the game first in the same Windows session/Wine prefix.");
            process::exit(1)
        }
    };
    let proc_handle=unsafe{OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,0,pid)};
    if proc_handle.is_null() {
        eprintln!("Cannot open Windows PID {} (Win32 error {}). Is the OEM game still running in the same Windows session/Wine prefix?",pid,unsafe{GetLastError()});
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
    let mut crt_result_seen=false;
    let mut crt_exit_seen=false;
    let mut stage=TraceStage::WinMain;
    let mut ended=false;
    let mut timeout_reported=false;
    loop {
        let mut ev:DebugEvent=unsafe{zeroed()};
        if unsafe{WaitForDebugEvent(&mut ev,30000)}==0{
            let err=unsafe{GetLastError()};
            if err==121 { // ERROR_SEM_TIMEOUT: expected when the game is still running.
                if !timeout_reported {
                    println!("WAITING_FOR_GAME_QUIT debugger wait timeout is normal while game runs.");
                    timeout_reported=true;
                }
                continue;
            }
            eprintln!("WaitForDebugEvent failed (Win32 error {}); detaching safely.",err);
            break;
        }
        timeout_reported=false;
        let mut disposition=DBG_CONTINUE;
        match ev.kind {
            CREATE_THREAD_DEBUG_EVENT|CREATE_PROCESS_DEBUG_EVENT=>{
                let ok=configure_thread(ev.tid,with_peeks,stage);
                threads.insert(ev.tid,ok);
                if !ok{eprintln!("WARN: unable to set hardware breakpoints for thread {}.",ev.tid);}
            }
            EXIT_THREAD_DEBUG_EVENT=>{threads.remove(&ev.tid);}
            EXIT_PROCESS_DEBUG_EVENT=>{
                let code=u32::from_le_bytes(ev.payload[0..4].try_into().unwrap());
                println!("PROCESS_EXIT raw_code={} (0x{:08x}) captured_winmain={} crt_result_seen={} crt_exit_seen={}",code,code,captured,crt_result_seen,crt_exit_seen);
                ended=true;
            }
            EXCEPTION_DEBUG_EVENT=>{
                let exception=u32::from_le_bytes(ev.payload[0..4].try_into().unwrap());
                if exception==EXCEPTION_SINGLE_STEP {
                    let h=unsafe{OpenThread(THREAD_GET_CONTEXT,0,ev.tid)};
                    let c=if h.is_null(){None}else{context(h)};
                    if !h.is_null(){unsafe{CloseHandle(h);}}
                    match c {
                      Some(c) if stage==TraceStage::WinMain && c.eip==PEEK_RETURN_INSN && with_peeks=>{
                        if c.eax!=0 {
                            if let Some(m)=read_msg(proc_handle,c.esp){
                                if last.len()==12{last.pop_front();}
                                last.push_back(Seen{msg:m,tid:ev.tid,peek_eax:c.eax});
                            }
                        }
                      }
                      Some(c) if stage==TraceStage::WinMain && c.eip==EXIT_INSN=>{
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
                        // Follow the real CRT/Win32 exit path, rather than
                        // predicting the process code and detaching early.
                        stage=TraceStage::CrtExit;
                        for (&tid,_) in &threads {
                            if !configure_thread(tid,with_peeks,stage) {
                                eprintln!("WARN: could not arm CRT breakpoints for thread {}",tid);
                            }
                        }
                      }
                      Some(c) if stage==TraceStage::CrtExit && c.eip==CRT_WINMAIN_RESULT_INSN=>{
                        crt_result_seen=true;
                        println!("CRT_WINMAIN_RESULT tid={} eax={} (0x{:08x}); original CRT about to push WinMain return",ev.tid,c.eax,c.eax);
                        // The breakpoint is on the *next* instruction to execute.
                        // Remove it before continuing to avoid immediate retrigger.
                        stage=TraceStage::CrtExitArgument;
                        for (&tid,_) in &threads {
                            if !configure_thread(tid,with_peeks,stage) {
                                eprintln!("WARN: could not arm ExitProcess-argument breakpoint for thread {}",tid);
                            }
                        }
                      }
                      Some(c) if (stage==TraceStage::CrtExit || stage==TraceStage::CrtExitArgument) && c.eip==CRT_EXITPROCESS_ARGUMENT_INSN=>{
                        crt_exit_seen=true;
                        println!("CRT_EXITPROCESS_ARGUMENT tid={} esi={} (0x{:08x}) before push esi / imported ExitProcess",ev.tid,c.esi,c.esi);
                        stage=TraceStage::ProcessExit;
                        for (&tid,_) in &threads {
                            if !configure_thread(tid,with_peeks,stage) {
                                eprintln!("WARN: could not clear exit breakpoints for thread {}",tid);
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
        if ended{break;}
    }
    if !ended {
        // On any unexpected debugger failure, clear DR0/DR1 before detach.
        for (&tid,_) in &threads {
            if !configure_thread(tid,with_peeks,TraceStage::ProcessExit) {
                eprintln!("WARN: cannot clear hardware breakpoints for thread {}",tid);
            }
        }
        if unsafe{DebugActiveProcessStop(pid)}==0{
            eprintln!("WARN: debugger detach failed; game process may have already ended.");
        }else{println!("DETACHED (game continues with its original code and exit path).");}
    }
    unsafe{CloseHandle(proc_handle);}
}
