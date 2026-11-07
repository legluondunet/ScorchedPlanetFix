#!/usr/bin/env python3
"""Scorched Planet-specific, pinned GameFrame fix: native desktop 4:3, one game window, 60 FPS.

Applied only to an ephemeral checkout of tanepiper/game-frame at its exact pinned
commit in GitHub Actions, never to the upstream source or CyberGladiatorsFix.
Hard fail if upstream changes any patch anchor: prevent silent incompatible fixes.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2] / "third_party/game-frame/wrappers/glide/src"


def replace(path: str, before: str, after: str) -> None:
    file = ROOT / path
    data = file.read_text(encoding="utf-8")
    count = data.count(before)
    if count != 1:
        raise RuntimeError(f"Pin mismatch {path}: expected one anchor, found {count}: {before[:90]!r}")
    file.write_text(data.replace(before, after, 1), encoding="utf-8")


def main() -> None:
    # No ChangeDisplaySettings/ChangeDisplaySettingsEx hooks are needed:
    # GameFrame is a borderless OpenGL wrapper and never switches screen modes.
    replace("win32.rs", "    pub fn ShowWindow(w: HWND, cmd: i32) -> BOOL;",
            "    pub fn ShowWindow(w: HWND, cmd: i32) -> BOOL;\n"
            "    pub fn IsWindow(w: HWND) -> BOOL;\n"
            "    pub fn GetWindowTextA(w: HWND, name: *mut u8, max: i32) -> i32;\n"
            "    pub fn GetSystemMetrics(index: i32) -> i32;")

    replace("window.rs", "use std::ptr::null_mut;",
            """use std::ptr::null_mut;
use std::sync::atomic::{AtomicUsize, Ordering};

/// The early fallback surface is created before this particular game has a HWND.
static OWN: AtomicUsize = AtomicUsize::new(0);
/// Game-provided HWND onto which the renderer has moved; keep checking if destroyed.
static ADOPTED: AtomicUsize = AtomicUsize::new(0);

/// No display-mode switching, only size the client to the *existing* desktop.
fn desktop_size(w: u32, h: u32) -> (u32, u32) {
    if std::env::var_os("SCORCHED_GLIDE_DESKTOP").is_some() {
        let (sw, sh) = unsafe { (GetSystemMetrics(0), GetSystemMetrics(1)) };
        if sw > 0 && sh > 0 { return (sw as u32, sh as u32); }
    }
    (w, h)
}

/// The original Scorched Planet window is created *after* grSstOpen and is
/// also the keyboard/mouse input target. Adopt it on a later frame instead
/// of leaving OpenGL in a separate fallback window.
unsafe extern "system" fn find_scorched(w: HWND, out: isize) -> BOOL {
    let mut pid=0;
    unsafe { GetWindowThreadProcessId(w, &mut pid) };
    if pid!=unsafe { GetCurrentProcessId() } || unsafe { IsWindowVisible(w) }==0 {
        return 1;
    }
    let mut title=[0u8;128];
    let n=unsafe { GetWindowTextA(w,title.as_mut_ptr(),title.len() as i32) }.max(0) as usize;
    if &title[..n]!=b"Scorched Planet" { return 1; }
    unsafe { *(out as *mut HWND)=w; }
    0
}

/// Called on the game thread at grBufferSwap, while it owns the HWND.
/// Returns a different HWND once to enqueue a Cmd::Open (render context move).
pub fn adopt_scorched(w: u32, h: u32) -> Option<HWND> {
    if std::env::var_os("SCORCHED_GLIDE_ADOPT_WINDOW").is_none() { return None; }
    let previous=ADOPTED.load(Ordering::Relaxed) as HWND;
    if !previous.is_null() && unsafe { IsWindow(previous) }!=0 {
        return None;
    }
    let mut game: HWND=null_mut();
    unsafe { EnumWindows(find_scorched,&mut game as *mut _ as isize); }
    if game.is_null() { return None; }
    let sized=size(game,w,h);
    ADOPTED.store(sized as usize,Ordering::Relaxed);
    let fallback=OWN.load(Ordering::Relaxed) as HWND;
    if !fallback.is_null() && fallback!=sized {
        unsafe { ShowWindow(fallback,0); }
    }
    log!("SCORCHED_WINDOW: adopted original game HWND {sized:?}; fallback hidden; input and OpenGL on one HWND");
    Some(sized)
}
""")
    # Exclude our fallback HWND from the game's real window search. Otherwise
    # the first visible window can always be the fallback and never switches.
    replace("window.rs",
            "    let found = unsafe { &mut *(out as *mut (HWND, bool)) };",
            """    if &name[..n]==b"GlideWrapper" { return 1; } // never treat fallback as the game's window
    let found = unsafe { &mut *(out as *mut (HWND, bool)) };""")
    replace("window.rs",
            "pub fn prepare(w: u32, h: u32) -> HWND {\n"
            "    static OWN: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);",
            "pub fn prepare(w: u32, h: u32) -> HWND {\n"
            "    let (w,h)=desktop_size(w,h);")
    replace("window.rs",
            "pub fn size(hwnd: HWND, w: u32, h: u32) -> HWND {\n    unsafe {",
            "pub fn size(hwnd: HWND, w: u32, h: u32) -> HWND {\n"
            "    let (w,h)=desktop_size(w,h);\n    unsafe {")

    # Preserve exact original Glide framebuffer dimensions, change only window
    # and pacing. Queue a second Open command on the game's normal frame loop
    # after it creates its real HWND. The render thread already supports moving
    # its OpenGL context to a new HWND in Renderer::open.
    replace("api/sst.rs",
            "use std::sync::atomic::{AtomicU32, Ordering};",
            """use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::{Duration, Instant};

fn limit_scorched_fps() {
    // Limit the game's calls themselves (and thus game time), not only the
    // OpenGL presenter. A 120 Hz VSync would otherwise run the game at 120.
    if std::env::var_os("SCORCHED_GLIDE_FPS_60").is_none() { return; }
    static NEXT: OnceLock<Mutex<Option<Instant>>> = OnceLock::new();
    let clock=NEXT.get_or_init(||Mutex::new(None));
    let mut next=clock.lock().unwrap_or_else(|e|e.into_inner());
    let now=Instant::now();
    if let Some(deadline)=*next {
        if deadline>now { std::thread::sleep(deadline.duration_since(now)); }
    }
    // Do not try to catch up missed frames; cap independent of monitor Hz.
    *next=Some(Instant::now()+Duration::from_nanos(16_666_667));
}
""")
    replace("api/sst.rs",
            """    super::lfb::finish_write();
    push(Cmd::Swap);
    flush();
    super::movie::frame();""",
            """    super::lfb::finish_write();
    if let Some(hwnd)=crate::window::adopt_scorched(
        crate::config::get().output(screen().0,screen().1).0,
        crate::config::get().output(screen().0,screen().1).1,
    ) {
        let (w,h)=screen();
        push(Cmd::Open { width:w, height:h, out:crate::config::get().output(w,h), hwnd:hwnd as usize });
        log!("SCORCHED_WINDOW: queued OpenGL context move onto native game/input window");
    }
    push(Cmd::Swap);
    flush();
    limit_scorched_fps();
    super::movie::frame();""")
    # The existing source has a unitary render thread, limiting game-frame
    # callbacks limits both rendering and original-game simulation speed.
    print("Pinned GameFrame Scorched Planet overrides applied: desktop borderless + native input HWND + 60 FPS")
    for file in ["window.rs","win32.rs","api/sst.rs"]:
        text=(ROOT/file).read_text()
        if "ChangeDisplaySettings" in text:
            raise RuntimeError(f"Forbidden display mode switch introduced in {file}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"ERROR: {error}",file=sys.stderr)
        raise
