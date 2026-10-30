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
        patch_enabled:bool,
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
                patch_enabled:false,fadelow_allowed:false,edition:"inconnue",language:1,
                phase:Phase::Ready,status:"Initialisation…".into(),
                progress:0.0,dialog:None,rx,tx,game_ready:false,stop_on_modal:false,detect:String::new()};
            app.scan_start();
            app
        }
        fn scan_start(&mut self) {
            let image=match iso_install::find_image_and_install(&self.root){
                Ok(image)=>image,
                Err(e)=>{
                    self.detect="Plusieurs images ISO détectées".into();
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
                if !self.install.is_dir() || !self.install.join("SPVOODOO.EXE").is_file() {
                    self.detect="Répertoire splanet incomplet".into();
                    self.stop_on_modal=true;
                    self.info("Installation non compatible",
                        "Le dossier splanet existe, mais SPVOODOO.EXE est absent. Aucune modification effectuée.",true);
                    return;
                }
                self.game_ready=true;
                let (edition,can_patch)=installed_edition(&self.install.join("SPVOODOO.EXE"));
                self.fadelow_allowed=can_patch;
                self.patch_enabled=can_patch;
                self.edition=edition;
                let configured=read_ini_language(&self.install.join("splanet.ini"));
                self.language=configured.filter(|id|edition_languages(edition).iter().any(|(n,_)|n==id))
                    .unwrap_or_else(||edition_default_language(edition));
                self.status="Installation splanet détectée".into();
                self.detect=if image.is_some() {
                    format!("Edition {edition} ; installation existante conservée (ISO ignorée)")
                }else{format!("Dossier splanet détecté ; édition {edition}")};
                self.logger.write(&format!("DETECT: {} ; FadeLow eligible={can_patch}",self.detect));
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
                self.stop_on_modal=true;
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
                                self.edition=match report.edition{
                                    "US"=>"US 1.08",
                                    "EN/ES/IT"=>"EN/ES/IT 1.06",
                                    "FR/DE"=>"FR/DE 1.06",
                                    _=>"inconnue",
                                };
                                self.language=edition_default_language(self.edition);
                                self.fadelow_allowed=matches!(report.edition,"US"|"EN/ES/IT"|"FR/DE");
                                self.patch_enabled=self.fadelow_allowed;
                                self.progress=1.0;
                                self.status="Extraction terminée".into();
                                self.detect=format!("Installation depuis ISO : édition {} reconnue",report.edition);
                                self.info("Installation réussie",
                                    format!("Edition {} installée : {} fichiers, {} octets.\nDossier : {}\n{}\nDétails dans ScorchedPlanetFix.log",
                                        report.edition,report.files,report.bytes,report.folder.display(),
                                        if self.fadelow_allowed {"FadeLow disponible."}else{"FadeLow non validé pour cet exécutable."}),false);
                            },
                            Err(e)=>{
                                self.progress=0.0;
                                self.stop_on_modal=true;
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
            if !self.game_ready {self.info("Installation introuvable","Le répertoire splanet n'est pas prêt.",true);return;}
            if !self.fadelow_allowed {
                self.info("Correctif indisponible","Exécutable non reconnu. Correctifs validés : FadeLow US, EN/ES/IT ; FadeLow + détection CD FR/DE. Aucun octet modifié.",true);
                return;
            }
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
                ui.label(RichText::new("Correctifs disponibles").size(18.0).strong());
                let fr_de=self.edition.starts_with("FR/DE");
                let patch_label=if fr_de {
                    "Correction du fondu (FadeLow) + détection CD (FR/DE)"
                } else {
                    "Correction du blocage du fondu de palette (FadeLow)"
                };
                let patch_info=if fr_de {
                    "Pour FR/DE, applique les deux modifications testées ensemble : seuil FadeLow et résultat du contrôle de présence CD. Ne remplace pas les fichiers du CD manquants."
                } else {DESCRIPTION};
                ui.add_enabled_ui(matches!(self.phase,Phase::Ready)&&self.fadelow_allowed,|ui|{
                    ui.checkbox(&mut self.patch_enabled,patch_label)
                        .on_hover_text(patch_info);
                });
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
                    if ui.add_enabled(available&&self.fadelow_allowed&&self.patch_enabled,egui::Button::new("Appliquer").min_size(egui::vec2(135.0,34.0))).clicked(){
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
            viewport:egui::ViewportBuilder::default().with_title(title.clone())
                .with_inner_size([620.0,395.0]).with_min_inner_size([560.0,370.0]),
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
