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
mod cue_image;
#[cfg(windows)]
mod cli;
#[cfg(windows)]
mod iso_install;
#[cfg(windows)]
mod logger;

#[cfg(windows)]
mod app {
    use super::{iso_install,logger::Logger};
    use eframe::egui::{self,Color32,RichText};
    use scorched_planet_fix::{self,Action,ORIGINAL_SHA256,FADELOW_SHA256};
    use sha2::{Digest,Sha256};
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


    /// Exact game executable hashes. FR/DE is eligible only for the combined
    /// FadeLow plus media-gate profile, never the earlier FadeLow-only trial.
    fn installed_edition(exe:&Path)->(&'static str,bool){
        let Ok(bytes)=fs::read(exe) else {return ("inconnue",false)};
        let hash=format!("{:x}",Sha256::digest(&bytes));
        if let Some(profile)=scorched_planet_fix::profile_from_hash(&hash) {
            return (profile.label,true);
        }
        ("inconnue",false)
    }

    const EN_LANGUAGES:&[(u8,&str)]=&[(1,"English"),(4,"Italiano"),(5,"Español")];
    const FR_LANGUAGES:&[(u8,&str)]=&[(2,"Français"),(3,"Deutsch")];
    const US_LANGUAGES:&[(u8,&str)]=&[(1,"English")];
    fn edition_languages(edition:&str)->&'static [(u8,&'static str)] {
        if edition.starts_with("EN/ES/IT") {EN_LANGUAGES}
        else if edition.starts_with("FR/DE") {FR_LANGUAGES}
        else {US_LANGUAGES}
    }
    fn edition_default_language(edition:&str)->u8 {
        edition_languages(edition)[0].0
    }
    fn read_ini_language(path:&Path)->Option<u8> {
        let bytes=fs::read(path).ok()?;
        let mut in_scorch=false;
        for line in bytes.split(|b|*b==b'\n') {
            let trimmed=line.trim_ascii();
            if trimmed.starts_with(b"[") && trimmed.ends_with(b"]") {
                in_scorch=trimmed.eq_ignore_ascii_case(b"[scorch]");
                continue;
            }
            if in_scorch && trimmed.len()>=9 && trimmed[..9].eq_ignore_ascii_case(b"language=") {
                return std::str::from_utf8(trimmed[9..].trim_ascii()).ok()?.parse().ok();
            }
        }
        None
    }

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
        fadelow_allowed:bool,
        edition:&'static str,
        language:u8,
        phase:Phase,
        status:String,
        progress:f32,
        dialog:Option<Modal>,
        rx:Receiver<Event>,
        tx:Sender<Event>,
        game_ready:bool,
        stop_on_modal:bool,
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
                fadelow_allowed:false,edition:"inconnue",language:1,
                phase:Phase::Ready,status:"Initialisation…".into(),
                progress:0.0,dialog:None,rx,tx,game_ready:false,stop_on_modal:false,detect:String::new()};
            app.scan_start();
            app
        }
        fn scan_start(&mut self) {
            let image=match iso_install::find_image_and_install(&self.root){
                Ok(image)=>image,
                Err(e)=>{
                    self.detect="Images ISO/CUE ambiguës ou incomplètes".into();
                    self.stop_on_modal=true;
                    self.info("Image du jeu non supportée",e,true);
                    return;
                }
            };
            if self.install.is_symlink() {
                self.stop_on_modal=true;
                self.info("Installation non compatible","Lien symbolique splanet refusé.",true);
                return;
            }
            if self.install.exists(){
                let exe=match scorched_planet_fix::find_installed_exe(&self.install) {
                    Ok(path)=>path,
                    Err(e)=>{
                        self.detect="Répertoire splanet incomplet ou ambigu".into();
                        self.stop_on_modal=true;
                        self.info("Installation non compatible",e,true);
                        return;
                    }
                };
                self.game_ready=false; // Enable Launch only after verified patch.
                let (edition,can_patch)=installed_edition(&exe);
                self.fadelow_allowed=can_patch;
                self.edition=edition;
                let configured=read_ini_language(&self.install.join("splanet.ini"));
                self.language=configured.filter(|id|edition_languages(edition).iter().any(|(n,_)|n==id))
                    .unwrap_or_else(||edition_default_language(edition));
                self.status="Installation splanet détectée".into();
                self.detect=if image.is_some() {
                    format!("Edition {edition} ; installation existante conservée (ISO ignorée)")
                }else{format!("Dossier splanet détecté ; édition {edition}")};
                self.logger.write(&format!("DETECT: {} ; auto-patch eligible={can_patch}",self.detect));
                if can_patch {
                    self.apply_patch();
                } else {
                    self.game_ready=false;
                    self.info("Exécutable non reconnu",
                        "Cette édition ne possède aucun correctif validé. Exécution désactivée pour éviter un jeu non fonctionnel.",true);
                }
                return;
            }
            if let Some(iso)=image {
                self.detect="Image ISO ou CUE/BIN détectée — validation et extraction…".into();
                self.phase=Phase::Installing;
                self.status="Validation de l'image…".into();
                self.logger.write("DETECT: ISO/CUE candidate; starting validation before extraction");
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
                self.detect="Ni image ISO/CUE ni dossier splanet trouvés".into();
                self.stop_on_modal=true;
                self.status="Placez une image ISO ou CUE/BIN, ou un dossier splanet, près du patcher".into();
                self.logger.write("DETECT: no ISO and no splanet folder");
                self.info("Aucune installation détectée",
                    "Placez un ISO ou un couple CUE/BIN de Scorched Planet (édition Windows 3dfx), ou le dossier splanet, près de ScorchedPlanetFix.exe puis relancez le programme.",true);
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
                                self.edition=match report.edition{
                                    "US"=>"US 1.08",
                                    "EN/ES/IT"=>"EN/ES/IT 1.06",
                                    "FR/DE"=>"FR/DE 1.06",
                                    _=>"inconnue",
                                };
                                self.language=edition_default_language(self.edition);
                                self.fadelow_allowed=true;
                                self.progress=1.0;
                                self.status="Installation et correctif terminés".into();
                                self.detect=format!("Edition {} : correctif appliqué automatiquement",report.edition);
                                self.info("Installation réussie",
                                    format!("Edition {} installée : {} fichiers, {} octets.\nDossier : {}\nspvoodoo.exe.bak sauvegardé avant le patch.\nCorrectif automatique appliqué.\nDétails dans ScorchedPlanetFix.log",
                                        report.edition,report.files,report.bytes,report.folder.display()),false);
                            },
                            Err(e)=>{
                                self.progress=0.0;
                                self.stop_on_modal=true;
                                self.status="Installation ou correctif impossible".into();
                                self.info("Cette image du jeu n'est pas supportée",format!("{e}\nConsultez ScorchedPlanetFix.log."),true);
                            },
                        }
                    },
                    Event::Patched(result)=>{
                        self.phase=Phase::Ready;
                        match result{
                            Ok(text)=>{
                                self.progress=1.0;
                                self.game_ready=true;
                                self.status="Correctif automatique vérifié".into();
                                self.info("Correctif automatique",format!("{text}\nSauvegarde : <nom de l'exécutable>.bak\nConsultez ScorchedPlanetFix.log."),false);
                            },
                            Err(e)=>{
                                self.progress=0.0;
                                self.game_ready=false;
                                self.status="Échec du correctif automatique".into();
                                self.info("Échec de l'opération",format!("{e}\nConsultez ScorchedPlanetFix.log."),true);
                            },
                        }
                    }
                }
            }
        }
        fn select_language(&mut self,selection:u8){
            if !edition_languages(self.edition).iter().any(|(id,_)|*id==selection) {
                self.info("Langue incompatible","Cette langue ne fait pas partie de l'édition du CD détectée.",true);
                return;
            }
            let ini=self.install.join("splanet.ini");
            let video=self.install.join("video");
            match iso_install::update_ini(&ini,&video,selection){
                Ok(())=>{
                    self.language=selection;
                    self.logger.write(&format!("INI: selected language={selection} for {}",self.edition));
                    self.status=format!("Langue du jeu mise à jour (code {selection})");
                },
                Err(e)=>self.info("Configuration impossible",format!("splanet.ini: {e}"),true),
            }
        }
        fn apply_patch(&mut self){
            if !self.install.is_dir() {self.info("Installation introuvable","Le répertoire splanet n'est pas prêt.",true);return;}
            if !self.fadelow_allowed {
                self.info("Correctif indisponible","Exécutable non reconnu. Correctifs validés : FadeLow US, EN/ES/IT ; FadeLow + détection CD FR/DE. Aucun octet modifié.",true);
                return;
            }

            self.phase=Phase::Patching;
            self.progress=0.0;
            self.status="Sauvegarde .bak et correction automatique…".into();
            let exe=match scorched_planet_fix::find_installed_exe(&self.install){
                Ok(path)=>path,
                Err(e)=>{self.info("Exécutable introuvable",e,true);return;}
            };
            let log=self.logger.clone();
            let tx=self.tx.clone();
            std::thread::spawn(move ||{
                log.write("PATCH-AUTO: auto applying exact-edition correction");
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
            let game=match scorched_planet_fix::find_installed_exe(&self.install){
                Ok(path)=>path,
                Err(e)=>{self.info("Impossible de lancer",e,true);return;}
            };
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
                if dismiss {
                    self.dialog=None;
                    if self.stop_on_modal {
                        root.ctx().send_viewport_cmd(egui::ViewportCommand::Close);
                    }
                }
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
                    ui.label(RichText::new(self.version.as_str()).size(13.0).color(Color32::LIGHT_GRAY));
                    ui.label("Licence GNU GPL-3.0-or-later");
                });
                ui.add_space(18.0);
                ui.separator();
                ui.add_space(12.0);
                ui.label(RichText::new("Correctif automatique").size(18.0).strong());
                let fr_de=self.edition.starts_with("FR/DE");
                let patch_label=if fr_de {
                    "Correction du fondu (FadeLow) + détection CD (FR/DE)"
                } else {
                    "Correction du blocage du fondu de palette (FadeLow)"
                };
                let patch_info=if fr_de {
                    "Pour FR/DE, applique les deux modifications testées ensemble : seuil FadeLow et résultat du contrôle de présence CD. Ne remplace pas les fichiers du CD manquants."
                } else {DESCRIPTION};
                ui.label(patch_label).on_hover_text(patch_info);
                ui.label(RichText::new("Appliqué après extraction, avant génération de l'INI, avec sauvegarde .bak.")
                    .small().color(Color32::LIGHT_GRAY));
                if self.game_ready && !self.fadelow_allowed {
                    ui.label(RichText::new("FadeLow non validé pour cet exécutable ; application désactivée.")
                        .small().color(Color32::GRAY));
                }
                ui.add_space(12.0);
                if self.game_ready {
                    let choices=edition_languages(self.edition);
                    let mut selected=self.language;
                    ui.add_enabled_ui(matches!(self.phase,Phase::Ready),|ui|{
                        egui::ComboBox::from_label("Langue du jeu")
                            .selected_text(choices.iter().find(|(id,_)|*id==selected)
                                .map(|(_,label)|*label).unwrap_or("Inconnue"))
                            .show_ui(ui,|ui|{
                                for &(id,label) in choices {
                                    ui.selectable_value(&mut selected,id,label);
                                }
                            });
                    });
                    if selected!=self.language {self.select_language(selected);}
                    ui.label(RichText::new("Vidéos locales ; HighQualityVideo=1. Autres réglages conservés.")
                        .small().color(Color32::GRAY));
                }
                ui.add_space(10.0);
                ui.label(RichText::new(self.detect.as_str()).color(Color32::GRAY));
                ui.label(&self.status);
                if !matches!(self.phase,Phase::Ready){
                    ui.add(egui::ProgressBar::new(self.progress).show_percentage().desired_width(ui.available_width()));
                }
                ui.add_space(16.0);
                ui.horizontal(|ui|{
                    let available=matches!(self.phase,Phase::Ready)&&self.game_ready;
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
            viewport:egui::ViewportBuilder::default().with_title(title.clone())
                .with_inner_size([620.0,395.0]).with_min_inner_size([560.0,370.0]),
            ..Default::default()
        };
        eframe::run_native(&title,options,Box::new(move |_cc|{
            Ok(Box::new(ScorchedApp::new(root,log,version)))
        })).map_err(|e|format!("Erreur de l'interface graphique : {e}"))
    }
}
// The binary remains Windows GUI subsystem (no console window during normal
// GUI launch). In --nogui mode attach to a calling cmd/PowerShell console
// when available; also preserve normal redirected stdout and stderr.
#[cfg(windows)]
fn cli_console_write(message:&str,error:bool){
    use std::ffi::c_void;
    #[link(name="kernel32")]
    extern "system"{
        fn AttachConsole(process_id:u32)->i32;
        fn GetStdHandle(kind:u32)->*mut c_void;
        fn GetConsoleMode(handle:*mut c_void,mode:*mut u32)->i32;
        fn WriteConsoleW(handle:*mut c_void,buffer:*const u16,chars:u32,
            written:*mut u32,reserved:*mut c_void)->i32;
        fn WriteFile(handle:*mut c_void,buffer:*const u8,bytes:u32,
            written:*mut u32,overlapped:*mut c_void)->i32;
    }
    let text=format!("{message}\r\n");
    unsafe{
        let _=AttachConsole(u32::MAX); // ATTACH_PARENT_PROCESS
        let handle=GetStdHandle(if error {u32::MAX-11}else{u32::MAX-10});
        if handle.is_null() || handle as isize == -1 {return;}
        let mut mode=0;
        let mut count=0;
        if GetConsoleMode(handle,&mut mode)!=0{
            let utf16:Vec<u16>=text.encode_utf16().collect();
            let _=WriteConsoleW(handle,utf16.as_ptr(),utf16.len() as u32,
                &mut count,std::ptr::null_mut());
        }else{
            let bytes=text.as_bytes();
            let _=WriteFile(handle,bytes.as_ptr(),bytes.len() as u32,
                &mut count,std::ptr::null_mut());
        }
    }
}
#[cfg(windows)]
fn main(){
    let args:Vec<std::ffi::OsString>=std::env::args_os().skip(1).collect();
    if !args.is_empty(){
        if args[0]=="--nogui"{
            let result=(||{
                let current=std::env::current_dir()
                    .map_err(|e|format!("Répertoire courant inaccessible : {e}"))?;
                let version=format!("1.0.0-{}",env!("SCORCHED_COMMIT"));
                let logger=logger::Logger::new(&current,&version)?;
                logger.system_information();
                cli::run(&args[1..],&current,&logger,|line|cli_console_write(line,false))
                    .map_err(|e|{
                        logger.write(&format!("CLI ERROR: {e}"));
                        e
                    })
            })();
            if let Err(message)=result{
                cli_console_write(&format!("ScorchedPlanetFix : erreur : {message}"),true);
                std::process::exit(1);
            }
            return;
        }
        cli_console_write("Argument inconnu. Utilisez --nogui [image.iso | image.cue [image.bin]]",true);
        std::process::exit(2);
    }
    if let Err(e)=app::start(){
        // The GUI cannot be started (or its log could not be opened).
        use std::ffi::c_void;
        #[link(name="user32")]
        extern "system"{
            fn MessageBoxW(hwnd:*mut c_void,text:*const u16,caption:*const u16,flags:u32)->i32;
        }
        let msg:Vec<u16>=e.encode_utf16().chain(std::iter::once(0)).collect();
        let cap:Vec<u16>="ScorchedPlanetFix — erreur".encode_utf16()
            .chain(std::iter::once(0)).collect();
        unsafe{MessageBoxW(std::ptr::null_mut(),msg.as_ptr(),cap.as_ptr(),0x10);}
    }
}
