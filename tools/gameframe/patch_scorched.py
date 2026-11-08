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

    # The menu itself is not identified yet. Capture a low-overhead, opt-in
    # Glide-frame profile first; never infer "menu" from frame rate alone.
    # Profiles go to the existing unified launcher log through GLIDE_LOG.
    replace("api/sst.rs",
            "static WIDTH: AtomicU32 = AtomicU32::new(640);",
            """static WIDTH: AtomicU32 = AtomicU32::new(640);
static SCORCHED_PASSTHRU: AtomicU32 = AtomicU32::new(0);
pub fn scorched_passthru() -> u32 {
    SCORCHED_PASSTHRU.load(Ordering::Relaxed)
}""")
    replace("api/sst.rs",
            '    log!("grSstPassthruMode {mode}");',
            '    SCORCHED_PASSTHRU.store(mode, Ordering::Relaxed);\n    log!("grSstPassthruMode {mode}");')

    replace("render/mod.rs", "struct Renderer {", """/// Temporary frame-type probe. It makes NO assertion that 'idle' means menu.
struct ScorchedMenuProbe {
    enabled: bool,
    since: std::time::Instant,
    frames: u32,
    kinds: [u32; 3], // frames containing 3D, LFB-only, or neither
    totals: [u64; 3], // 3D primitives, LFB images, clears
    current: [u32; 3],
    last_kind: &'static str,
}

impl ScorchedMenuProbe {
    fn new() -> Self {
        let enabled=std::env::var_os("SCORCHED_MENU_PROBE").is_some();
        if enabled {
            log!("SCORCHED_MENU_PROBE: begin signature={} (telemetry only; no automatic menu classification)",
                std::env::var("SCORCHED_FIX_SIGNATURE").unwrap_or_else(|_| "unknown".into()));
        }
        Self {
            enabled, since: std::time::Instant::now(), frames: 0,
            kinds: [0;3], totals: [0;3], current: [0;3], last_kind: "unknown",
        }
    }

    fn record(&mut self, cmd: &Cmd) {
        if !self.enabled { return; }
        match cmd {
            Cmd::Triangle(_) | Cmd::Line(_) | Cmd::Point(_) => self.current[0]+=1,
            Cmd::Lfb(_) => self.current[1]+=1,
            Cmd::Clear { .. } => self.current[2]+=1,
            Cmd::Swap => {
                self.frames+=1;
                let kind=if self.current[0]>0 {"3d"}
                    else if self.current[1]>0 {"lfb-only"} else {"no-primitives"};
                let index=if kind=="3d" {0} else if kind=="lfb-only" {1} else {2};
                self.kinds[index]+=1;
                for (i,v) in self.current.iter().enumerate() { self.totals[i]+=*v as u64; }
                if kind!=self.last_kind {
                    log!("SCORCHED_MENU_PROBE: transition={} passthru={} frame_calls=3d:{},lfb:{},clear:{}",
                        kind,crate::api::sst::scorched_passthru(),
                        self.current[0],self.current[1],self.current[2]);
                    self.last_kind=kind;
                }
                self.current=[0;3];
                let elapsed=self.since.elapsed();
                if elapsed>=std::time::Duration::from_secs(2) {
                    log!("SCORCHED_MENU_PROBE: interval_ms={} frames={} passthru={} kinds=3d:{},lfb-only:{},no-primitives:{} commands=3d:{},lfb:{},clear:{}",
                        elapsed.as_millis(),self.frames,crate::api::sst::scorched_passthru(),
                        self.kinds[0],self.kinds[1],self.kinds[2],
                        self.totals[0],self.totals[1],self.totals[2]);
                    self.since=std::time::Instant::now();
                    self.frames=0;
                    self.kinds=[0;3];
                    self.totals=[0;3];
                }
            }
            _ => {}
        }
    }
}

struct Renderer {""")
    replace("render/mod.rs",
            "    spent: [std::time::Duration; 5],",
            "    spent: [std::time::Duration; 5],\n    scorched_menu_probe: ScorchedMenuProbe,")
    replace("render/mod.rs",
            "        spent: [std::time::Duration::ZERO; 5],",
            "        spent: [std::time::Duration::ZERO; 5],\n        scorched_menu_probe: ScorchedMenuProbe::new(),")
    replace("render/mod.rs",
            "            unsafe { r.exec(c) };",
            "            r.scorched_menu_probe.record(&c);\n            unsafe { r.exec(c) };")


    # Bitmap text and conservative four-region visual fingerprint for the
    # original OPTIONS root menu. The supplied screenshot is the calibration
    # image, but no game artwork is copied into distributed artifacts.
    (ROOT / "render" / "menu_overlay.rs").write_text(r'''//! Menu-only presentation overlay. Nothing written into original game buffers.
//! Color-shape detection is specific to the verified original 4:3 root menu.
use crate::gl::*;
use crate::render::shaders::Program;
use crate::log::log;

pub struct MenuOverlay {
    tex: u32, bitmap: Vec<u8>, width:i32, height:i32,
    ticks:u32, hits:u32, visible:bool,
}
fn glyph(c:char)->[u8;7]{
    match c {
        'S'=>[14,17,16,14,1,17,14], 'P'=>[30,17,17,30,16,16,16],
        'F'=>[31,16,16,30,16,16,16],
        'c'=>[0,0,14,17,16,17,14], 'o'=>[0,0,14,17,17,17,14],
        'r'=>[0,0,22,25,16,16,16], 'h'=>[16,16,30,17,17,17,17],
        'e'=>[0,0,14,17,31,16,14], 'd'=>[1,1,15,17,17,17,15],
        'l'=>[12,4,4,4,4,4,14], 'a'=>[0,0,14,1,15,17,15],
        'n'=>[0,0,30,17,17,17,17], 't'=>[8,8,28,8,8,9,6],
        'i'=>[4,0,12,4,4,4,14], 'x'=>[0,0,17,10,4,10,17],
        'f'=>[6,9,8,28,8,8,8], 'b'=>[16,16,30,17,17,17,30],
        '0'=>[14,17,19,21,25,17,14], '1'=>[4,12,4,4,4,4,14],
        '2'=>[14,17,1,2,4,8,31], '3'=>[30,1,1,14,1,1,30],
        '4'=>[2,6,10,18,31,2,2], '5'=>[31,16,30,1,1,17,14],
        '6'=>[6,8,16,30,17,17,14], '7'=>[31,1,2,4,8,8,8],
        '8'=>[14,17,17,14,17,17,14], '9'=>[14,17,17,15,1,2,12],
        '-'=>[0,0,0,31,0,0,0], '.'=>[0,0,0,0,0,12,12],
        ' '=>[0;7], _=>[31,17,2,4,4,0,4],
    }
}
fn text_image(text:&str)->(i32,i32,Vec<u8>){
    let chars:Vec<char>=text.chars().take(72).collect();
    let w=chars.len() as i32*12;
    let h=14;
    let mut pixels=vec![0u8;w as usize*h as usize*4];
    for (i,c) in chars.iter().enumerate(){
        for (y,mask) in glyph(*c).iter().enumerate(){
            for x in 0..5usize {
                if (mask&(1<<(4-x)))==0 {continue;}
                for dy in 0..2 {for dx in 0..2 {
                    let px=i*12+x*2+dx;let py=y*2+dy;
                    let at=(py*w as usize+px)*4;
                    pixels[at..at+4].copy_from_slice(&[247,198,43,255]);
                }}
            }
        }
    }
    (w,h,pixels)
}

#[derive(Clone,Copy)]
enum Color { Yellow, Blue, White }
unsafe fn color_fraction(gl:&Gl,cw:i32,ch:i32,x0:i32,y0:i32,vw:i32,vh:i32,
                         u:f32,v:f32,want:Color)->f32{
    // Four tiny color-family checks, normalized to the active centered 4:3 image.
    let side=((vw as f32*0.01).round() as i32).clamp(8,18);
    let px=x0+(vw as f32*u).round() as i32;
    let py=y0+vh-(vh as f32*v).round() as i32-side;
    if px<0||py<0||px+side>cw||py+side>ch{return 0.;}
    let mut rgba=vec![0u8;(side*side*4) as usize];
    unsafe{(gl.ReadPixels)(px,py,side,side,RGBA,UNSIGNED_BYTE,rgba.as_mut_ptr().cast());}
    let hits=rgba.chunks_exact(4).filter(|c|{
        let (r,g,b)=(c[0] as u32,c[1] as u32,c[2] as u32);
        match want {
            Color::Yellow=>r>140&&g>75&&b<80,
            Color::Blue=>b>85&&b*5>r*6&&g>15,
            Color::White=>r>165&&g>150&&b>130,
        }
    }).count();
    hits as f32/(side*side) as f32
}
unsafe fn root_menu(gl:&Gl,cw:i32,ch:i32,x0:i32,y0:i32,vw:i32,vh:i32)->bool{
    if vw<640||vh<480{return false;}
    unsafe{(gl.BindFramebuffer)(READ_FRAMEBUFFER,0);}
    // Fingerprints from maintainer screenshot: yellow OPTIONS heading, globe,
    // New Game panel, Quit panel. All required to avoid gameplay false positives.
    let yellow=unsafe{color_fraction(gl,cw,ch,x0,y0,vw,vh,0.290,0.130,Color::Yellow)};
    let globe=unsafe{color_fraction(gl,cw,ch,x0,y0,vw,vh,0.864,0.400,Color::Blue)};
    let new_game=unsafe{color_fraction(gl,cw,ch,x0,y0,vw,vh,0.093,0.394,Color::White)};
    let quit=unsafe{color_fraction(gl,cw,ch,x0,y0,vw,vh,0.864,0.752,Color::White)};
    // Low-frequency calibration record for color/DPI differences under Wine.
    static SAMPLES:std::sync::atomic::AtomicU32=std::sync::atomic::AtomicU32::new(0);
    if SAMPLES.fetch_add(1,std::sync::atomic::Ordering::Relaxed)%120==0 {
        log!("SCORCHED_MENU_OVERLAY: scores title={:.2} globe={:.2} new={:.2} quit={:.2}",
            yellow,globe,new_game,quit);
    }
    yellow>0.16&&globe>0.07&&new_game>0.08&&quit>0.08
}
impl MenuOverlay {
    pub fn new()->Self{
        let signature=std::env::var("SCORCHED_FIX_SIGNATURE").unwrap_or_default();
        let (width,height,bitmap)=text_image(&signature);
        log!("SCORCHED_MENU_OVERLAY: label={} root-menu recognition enabled (fail-closed)",signature);
        Self{tex:0,bitmap,width,height,ticks:0,hits:0,visible:false}
    }
    pub unsafe fn draw(&mut self,gl:&Gl,lfb:&Program,
                       cw:i32,ch:i32,x0:i32,y0:i32,vw:i32,vh:i32){
        if self.bitmap.is_empty()||self.width<=0||cw<=0||ch<=0{return;}
        self.ticks=self.ticks.wrapping_add(1);
        // Hidden: sample every 3 frames. Visible: sample each frame to hide
        // immediately on scene changes.
        if self.visible||self.ticks%3==0 {
            let is_menu=unsafe{root_menu(gl,cw,ch,x0,y0,vw,vh)};
            self.hits=if is_menu {self.hits.saturating_add(1)} else {0};
            let now=self.hits>=2;
            if now!=self.visible {
                self.visible=now;
                log!("SCORCHED_MENU_OVERLAY: visible={now}");
            }
        }
        if !self.visible{return;}
        unsafe {
            if self.tex==0 {
                (gl.GenTextures)(1,&mut self.tex);
                (gl.BindTexture)(TEXTURE_2D,self.tex);
                (gl.TexImage2D)(TEXTURE_2D,0,RGBA8 as i32,self.width,self.height,
                    0,RGBA,UNSIGNED_BYTE,self.bitmap.as_ptr().cast());
                (gl.TexParameteri)(TEXTURE_2D,TEXTURE_MIN_FILTER,NEAREST);
                (gl.TexParameteri)(TEXTURE_2D,TEXTURE_MAG_FILTER,NEAREST);
                (gl.TexParameteri)(TEXTURE_2D,TEXTURE_WRAP_S,CLAMP_TO_EDGE);
                (gl.TexParameteri)(TEXTURE_2D,TEXTURE_WRAP_T,CLAMP_TO_EDGE);
            }
            (gl.BindFramebuffer)(FRAMEBUFFER,0);
            (gl.Disable)(SCISSOR_TEST);
            (gl.Disable)(DEPTH_TEST);
            (gl.DepthMask)(0);
            (gl.ColorMask)(1,1,1,1);
            (gl.Enable)(BLEND);
            (gl.BlendFuncSeparate)(0x0302,0x0303,1,0x0303);
            (gl.Viewport)(0,0,cw,ch);
            (gl.UseProgram)(lfb.id);
            (gl.ActiveTexture)(TEXTURE0);
            (gl.BindTexture)(TEXTURE_2D,self.tex);
            (gl.Uniform1i)(lfb.loc("uImg"),0);
            (gl.Uniform1i)(lfb.loc("uChroma"),0);
            (gl.Uniform2f)(lfb.loc("uScreen"),cw as f32,ch as f32);
            // Anchor to the bottom-right of the presentation surface, not
            // the active 4:3 game image: preserve every original menu element.
            // Fixed pixel margins and text size stay consistent on Wine/Windows.
            let left=(cw-self.width-14).max(0) as f32;
            let top=(ch-self.height-12).max(0) as f32;
            (gl.Uniform4f)(lfb.loc("uRect"),left,top,
                left+self.width as f32,top+self.height as f32);
            (gl.DrawArrays)(0x0005,0,4);
            (gl.Disable)(BLEND);
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn embedded_glyphs_not_game_images() {
        let (w,h,b)=text_image("ScorchedPlanetFix 2.0.0-deadbeef");
        assert!(w>0&&h==14&&b.len()==(w*h*4) as usize);
        assert!(b.chunks_exact(4).any(|p|p[3]==255));
    }
}
''', encoding="utf-8")
    replace("render/mod.rs", "mod frame;", "mod frame;\nmod menu_overlay;")
    replace("render/mod.rs",
            "    spent: [std::time::Duration; 5],",
            "    spent: [std::time::Duration; 5],\n    menu_overlay: menu_overlay::MenuOverlay,")
    replace("render/mod.rs",
            "        spent: [std::time::Duration::ZERO; 5],",
            "        spent: [std::time::Duration::ZERO; 5],\n        menu_overlay: menu_overlay::MenuOverlay::new(),")
    replace("render/mod.rs",
            "unsafe { g.frame.present(&g.gl, &g.programs.present, self.state.gamma, cw, ch, g.dc) };",
            "unsafe { g.frame.present(&g.gl, &g.programs.present, &g.programs.lfb, &mut self.menu_overlay, self.state.gamma, cw, ch, g.dc) };")
    replace("render/mod.rs",
            "g.frame.show(&g.gl, &g.programs.present, self.state.gamma, cw, ch, g.dc, g.frame.back ^ 1)",
            "g.frame.show(&g.gl, &g.programs.present, &g.programs.lfb, &mut self.menu_overlay, self.state.gamma, cw, ch, g.dc, g.frame.back ^ 1)")
    replace("render/frame.rs",
            "pub unsafe fn present(&mut self, gl: &Gl, p: &Program, gamma: f32, cw: i32, ch: i32, dc: crate::win32::HDC)",
            "pub unsafe fn present(&mut self, gl: &Gl, p: &Program, lfb: &Program, overlay: &mut crate::render::menu_overlay::MenuOverlay, gamma: f32, cw: i32, ch: i32, dc: crate::win32::HDC)")
    replace("render/frame.rs",
            "self.show(gl, p, gamma, cw, ch, dc, self.back);",
            "self.show(gl, p, lfb, overlay, gamma, cw, ch, dc, self.back);")
    replace("render/frame.rs",
            "pub unsafe fn show(&self, gl: &Gl, p: &Program, gamma: f32, cw: i32, ch: i32, dc: crate::win32::HDC, which: usize)",
            "pub unsafe fn show(&self, gl: &Gl, p: &Program, lfb: &Program, overlay: &mut crate::render::menu_overlay::MenuOverlay, gamma: f32, cw: i32, ch: i32, dc: crate::win32::HDC, which: usize)")
    replace("render/frame.rs",
            '            crate::gl::check(gl, "in the frame");',
            '            overlay.draw(gl, lfb, cw, ch, x0, y0, vw, vh);\n            crate::gl::check(gl, "in the frame");')

    print("Pinned GameFrame Scorched Planet overrides applied: desktop borderless + native input HWND + 60 FPS + menu telemetry + root-menu version overlay")
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
