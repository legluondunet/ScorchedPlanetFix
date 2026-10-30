#![cfg_attr(windows, windows_subsystem = "windows")]
// SPDX-License-Identifier: GPL-3.0-or-later
//! Windows-only ScorchedPlanetFix GUI.
//! Distribution contains no proprietary game files, patches only exact 1996 EXE.
#[cfg(not(windows))]
compile_error!("ScorchedPlanetFix 1.0 GUI is Windows-only; run its Windows binary using Wine on Linux.");

#[cfg(windows)]
mod diagnostics;
#[cfg(windows)]
mod diagnostics_graphics;
#[cfg(windows)]
mod iso_install;
#[cfg(windows)]
mod logger;

#[cfg(windows)]
mod app {
    use super::{iso_install,logger::Logger};
    use eframe::egui::{self,Color32,RichText};
    use scorched_planet_fix::{self,Action,ORIGINAL_SHA256,FADELOW_SHA256};
    use std::fs;
    use std::path::{Path,PathBuf};
    use std::process::Command;
    use std::sync::mpsc::{self,Receiver,Sender};
    use std::time::Duration;

    const DESCRIPTION:&str="Corrige le blocage du fondu de palette lors du démarrage \
    d'une mission (bouton Go). Sur des processeurs rapides, le temporisateur \
    de Scorched Planet ignore les intervalles trop courts et le fondu \
    peut ne jamais se terminer. FadeLow réduit ce seuil, sans modifier \
    la résolution ni le fonctionnement du CD.";

    enum Event {
        Progress(u64,u64,String),
        Installed(Result<iso_install::InstallReport,String>),
        Patched(Result<String,String>),
    }
    struct Modal { title:String, text:String, error:bool }
    enum Phase { Ready, Installing, Patching }

    pub struct ScorchedApp {
        root:PathBuf,
        logger:Logger,
        version:String,
        install:PathBuf,
        patch_enabled:bool,
        phase:Phase,
        status:String,
        progress:f32,
        dialog:Option<Modal>,
        rx:Receiver<Event>,
        tx:Sender<Event>,
        game_ready:bool,
        detect: String,
    }
    impl ScorchedApp {
        fn info(&mut self,title:&str,message:impl Into<String>,error:bool){
            let message=message.into();
            self.logger.write(&format!("{}: {message}",if error{"ERROR"}else{"OK"}));
            self.dialog=Some(Modal { title:title.into(),text:message,error });
        }
        fn new(root:PathBuf,logger:Logger,version:String)->Self{
            let (tx,rx)=mpsc::channel();
            let install=root.join("splanet");
            let mut app=Self {root,logger,version,install,
                patch_enabled:true,phase:Phase::Ready,status:"Initialisation…".into(),
                progress:0.0,dialog:None,rx,tx,game_ready:false,detect:String::new()};
            app.scan_start();
            app
        }
        fn scan_start(&mut self) {
            let image=match iso_install::find_image_and_install(&self.root){
                Ok(image)=>image,
                Err(e)=>{
                    self.detect="Plusieurs images ISO détectées".into();
                    self.info("Image du jeu non supportée",e,true);
                    return;
                }
            };
            if self.install.exists(){
                if !self.install.is_dir() || !self.install.join("SPVOODOO.EXE").is_file() {
                    self.detect="Répertoire splanet incomplet".into();
                    self.info("Installation non compatible",
                        "Le dossier splanet existe, mais SPVOODOO.EXE est absent. Aucune modification effectuée.",true);
                    return;
                }
                self.game_ready=true;
                self.status="Installation splanet détectée".into();
                self.detect=if image.is_some() {
                    "Dossier splanet et ISO présents : installation existante conservée".into()
                }else{"Dossier splanet détecté".into()};
                self.logger.write(&format!("DETECT: {} ; EXE found",self.detect));
                return;
            }
            if let Some(iso)=image {
                self.detect="ISO détectée — validation et extraction…".into();
                self.phase=Phase::Installing;
                self.status="Validation de l'image…".into();
                self.logger.write("DETECT: supported candidate ISO; starting validation before extraction");
                let dest=self.install.clone();
                let tx=self.tx.clone();
                let log=self.logger.clone();
                std::thread::spawn(move || {
                    let tx_progress=tx.clone();
                    let result=iso_install::install_iso(&iso,&dest,
                        |done,total,label|{let _=tx_progress.send(Event::Progress(done,total,label.into()));},
                        |message|log.write(message));
                    if let Err(ref e)=result{log.write(&format!("ISO ERROR: {e}"));}
                    let _=tx.send(Event::Installed(result));
                });
            }else{
                self.detect="Ni image ISO ni dossier splanet trouvés".into();
                self.status="Placez l'ISO compatible ou un dossier splanet près du patcher".into();
                self.logger.write("DETECT: no ISO and no splanet folder");
                self.info("Aucune installation détectée",
                    "Placez une ISO de Scorched Planet (édition Windows 3dfx) ou le dossier splanet à côté de ScorchedPlanetFix.exe, puis relancez le programme.",true);
            }
        }
        fn progress_events(&mut self){
            while let Ok(event)=self.rx.try_recv(){
                match event{
                    Event::Progress(done,total,label)=>{
                        self.progress=if total>0 {(done as f32/total as f32).clamp(0.0,1.0)}else{0.0};
                        self.status=label;
                    },
                    Event::Installed(result)=>{
                        self.phase=Phase::Ready;
                        match result {
                            Ok(report)=>{
                                self.game_ready=true;
                                self.progress=1.0;
                                self.status="Extraction terminée".into();
                                self.detect="Installation splanet créée depuis l'ISO".into();
                                self.info("Installation réussie",
                                    format!("Installation terminée : {} fichiers, {} octets.\nDossier : {}\nDétails dans ScorchedPlanetFix.log",
                                        report.files,report.bytes,report.folder.display()),false);
                            },
                            Err(e)=>{
                                self.progress=0.0;
                                self.status="Extraction impossible".into();
                                self.info("Cette image du jeu n'est pas supportée",format!("{e}\nConsultez ScorchedPlanetFix.log."),true);
                            },
                        }
                    },
                    Event::Patched(result)=>{
                        self.phase=Phase::Ready;
                        match result{
                            Ok(text)=>{
                                self.progress=1.0;
                                self.status="Correctif appliqué".into();
                                self.info("Opération réussie",format!("{text}\nConsultez ScorchedPlanetFix.log pour les détails."),false);
                            },
                            Err(e)=>{
                                self.progress=0.0;
                                self.status="Échec de l'application".into();
                                self.info("Échec de l'opération",format!("{e}\nConsultez ScorchedPlanetFix.log."),true);
                            },
                        }
                    }
                }
            }
        }
        fn apply_patch(&mut self){
            if !self.game_ready {self.info("Installation introuvable","Le répertoire splanet n'est pas prêt.",true);return;}
            if !self.patch_enabled {self.info("Aucun correctif sélectionné","Cochez au moins un correctif avant de cliquer sur Appliquer.",true);return;}
            self.phase=Phase::Patching;
            self.progress=0.0;
            self.status="Vérification et application de FadeLow…".into();
            let exe=self.install.join("SPVOODOO.EXE");
            let log=self.logger.clone();
            let tx=self.tx.clone();
            std::thread::spawn(move ||{
                log.write("PATCH: FadeLow requested");
                let result=scorched_planet_fix::execute(Action::Apply,&exe,None,true);
                match &result{
                    Ok(text)=>log.write(&format!("PATCH OK: {text}")),
                    Err(e)=>log.write(&format!("PATCH ERROR: {e}")),
                }
                let _=tx.send(Event::Patched(result));
            });
        }
        fn launch_game(&mut self){
            if !self.game_ready {
                self.info("Impossible de lancer","Le jeu n'est pas installé.",true);return;
            }
            let game=self.install.join("SPVOODOO.EXE");
            if !game.is_file(){
                self.info("Impossible de lancer","SPVOODOO.EXE est absent.",true);return;
            }
            self.logger.write("LAUNCH: launching user's SPVOODOO.EXE, working directory splanet");
            match Command::new(&game).current_dir(&self.install).spawn(){
                Ok(child)=>{
                    self.status="Jeu lancé".into();
                    self.logger.write(&format!("LAUNCH OK: process ID {}",child.id()));
                },
                Err(e)=>{
                    self.info("Impossible de lancer",
                        format!("Démarrage du jeu échoué : {e}\nVérifiez que Glide (nGlide ou compatible) est disponible."),true);
                },
            }
        }
        fn show_dialog(&mut self,root:&egui::Ui){
            if let Some(dialog)=&self.dialog {
                let title=dialog.title.clone();
                let text=dialog.text.clone();
                let error=dialog.error;
                let mut dismiss=false;
                egui::Window::new(title)
                    .collapsible(false).resizable(false)
                    .anchor(egui::Align2::CENTER_CENTER,[0.0,0.0])
                    .show(root.ctx(),|ui|{
                        ui.set_min_width(360.0);
                        ui.label(RichText::new(&text).color(if error{Color32::from_rgb(231,113,113)}
                            else{Color32::from_rgb(145,220,155)}));
                        ui.add_space(10.0);
                        ui.vertical_centered(|ui|{if ui.button("OK").clicked(){dismiss=true;}});
                    });
                if dismiss {self.dialog=None;}
            }
        }
    }
    impl eframe::App for ScorchedApp {
        fn ui(&mut self, ui:&mut egui::Ui,_frame:&mut eframe::Frame){
            self.progress_events();
            if !matches!(self.phase,Phase::Ready) {
                ui.ctx().request_repaint_after(Duration::from_millis(70));
            }
            egui::CentralPanel::default().show(ui,|ui|{
                ui.add_space(16.0);
                ui.vertical_centered(|ui|{
                    ui.heading(RichText::new("ScorchedPlanetFix").size(28.0).strong());
                    ui.label(RichText::new(&self.version).size(13.0).color(Color32::LIGHT_GRAY));
                    ui.label("Licence GNU GPL-3.0-or-later");
                });
                ui.add_space(18.0);
                ui.separator();
                ui.add_space(12.0);
                ui.label(RichText::new("Correctifs disponibles").size(18.0).strong());
                ui.add_enabled_ui(matches!(self.phase,Phase::Ready),|ui|{
                    ui.checkbox(&mut self.patch_enabled,"Correction du blocage du fondu de palette (FadeLow)")
                        .on_hover_text(DESCRIPTION);
                });
                ui.add_space(16.0);
                ui.label(RichText::new(&self.detect).color(Color32::GRAY));
                ui.label(&self.status);
                if !matches!(self.phase,Phase::Ready){
                    ui.add(egui::ProgressBar::new(self.progress).show_percentage().desired_width(ui.available_width()));
                }
                ui.add_space(16.0);
                ui.horizontal(|ui|{
                    let available=matches!(self.phase,Phase::Ready)&&self.game_ready;
                    if ui.add_enabled(available,egui::Button::new("Appliquer").min_size(egui::vec2(135.0,34.0))).clicked(){
                        self.apply_patch();
                    }
                    if ui.add_enabled(available,egui::Button::new("Lancer").min_size(egui::vec2(135.0,34.0))).clicked(){
                        self.launch_game();
                    }
                });
                ui.add_space(10.0);
                ui.separator();
                ui.label(RichText::new("Journal : ScorchedPlanetFix.log (dans le dossier du patcher)").small().color(Color32::GRAY));
            });
            self.show_dialog(ui);
        }
    }

    pub fn start()->Result<(),String>{
        let executable=std::env::current_exe().map_err(|e|format!("Exécutable introuvable : {e}"))?;
        let root=executable.parent().ok_or("Dossier de l'exécutable absent")?.to_path_buf();
        let version=format!("1.0.0-{}",env!("SCORCHED_COMMIT"));
        let log=Logger::new(&root,&version)?;
        log.system_information();
        let title=format!("ScorchedPlanetFix {version}");
        let options=eframe::NativeOptions {
            viewport:egui::ViewportBuilder::default().with_title(&title)
                .with_inner_size([590.0,330.0]).with_min_inner_size([530.0,310.0]),
            ..Default::default()
        };
        eframe::run_native(&title,options,Box::new(move |_cc|{
            Ok(Box::new(ScorchedApp::new(root,log,version)))
        })).map_err(|e|format!("Erreur de l'interface graphique : {e}"))
    }
}
#[cfg(windows)]
fn main(){
    if let Err(e)=app::start(){
        // The GUI cannot be started (or its log could not be opened).
        // Avoid a silent failure when built as WINDOWS subsystem.
        use std::ffi::c_void;
        #[link(name="user32")]
        extern "system" {
            fn MessageBoxW(hwnd:*mut c_void,text:*const u16,caption:*const u16,flags:u32)->i32;
        }
        let msg:Vec<u16>=e.encode_utf16().chain(std::iter::once(0)).collect();
        let cap:Vec<u16>="ScorchedPlanetFix — erreur".encode_utf16().chain(std::iter::once(0)).collect();
        unsafe {MessageBoxW(std::ptr::null_mut(),msg.as_ptr(),cap.as_ptr(),0x10);}
    }
}
