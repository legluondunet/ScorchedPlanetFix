#![cfg_attr(target_os = "windows", windows_subsystem = "windows")]
//! Modern Windows setup UI. The commercial BMP assets are loaded from the
//! user's own CD directory at runtime, never embedded in this executable.

use eframe::egui::{self, Color32, TextureHandle};
use scorched_planet_setup::{audit_cd, execute_install, plan_install, read_cd_languages, CdLanguages, InstallOptions, InstallResult};
use std::path::{Path, PathBuf};
use std::sync::mpsc::{self, Receiver};
use std::time::Duration;
use walkdir::WalkDir;

#[derive(PartialEq, Eq, Clone, Copy)]
enum Language {
    English,
    French,
    German,
}
impl Language {
    fn code(self) -> u8 {
        match self {
            Self::English => 1,
            Self::French => 2,
            Self::German => 3,
        }
    }
    fn from_code(code: u8) -> Option<Self> {
        match code {
            1 => Some(Self::English),
            2 => Some(Self::French),
            3 => Some(Self::German),
            _ => None,
        }
    }
}

#[derive(PartialEq, Eq, Clone, Copy)]
enum Stage {
    Ready,
    Installing,
    Complete,
    Failed,
}

enum WorkerMessage {
    Progress(u64, u64, String),
    Complete(Result<InstallResult, String>),
}

struct SetupApp {
    source: String,
    destination: String,
    language: Language,
    stage: Stage,
    progress: f32,
    status: String,
    error: Option<String>,
    result: Option<InstallResult>,
    worker: Option<Receiver<WorkerMessage>>,
    banner: Option<TextureHandle>,
    illustration: Option<TextureHandle>,
    artwork_source: String,
    artwork_status: String,
    audit_status: String,
    cd_language_source: String,
    cd_languages: Option<CdLanguages>,
    cd_language_note: String,
}

fn original_art(root: &Path, candidates: &[&str]) -> Option<PathBuf> {
    let install_dir = std::fs::read_dir(root)
        .ok()?
        .flatten()
        .find(|entry| {
            entry
                .file_type()
                .is_ok_and(|ty| ty.is_dir())
                && entry
                    .file_name()
                    .to_string_lossy()
                    .eq_ignore_ascii_case("INSTALL")
        })?
        .path();
    for filename in candidates {
        for entry in WalkDir::new(&install_dir)
            .max_depth(5)
            .follow_links(false)
            .into_iter()
            .flatten()
        {
            if entry.file_type().is_file()
                && entry.file_name().to_string_lossy().eq_ignore_ascii_case(filename)
            {
                return Some(entry.path().to_owned());
            }
        }
    }
    None
}

fn load_texture(ctx: &egui::Context, file: &Path, id: &str) -> Option<TextureHandle> {
    let picture = image::open(file).ok()?.thumbnail(1400, 480).to_rgba8();
    let size = [picture.width() as usize, picture.height() as usize];
    let pixels = egui::ColorImage::from_rgba_unmultiplied(size, picture.as_raw());
    Some(ctx.load_texture(id, pixels, egui::TextureOptions::LINEAR))
}

fn show_texture(ui: &mut egui::Ui, texture: &TextureHandle, max_height: f32) {
    let native = texture.size_vec2();
    let available = ui.available_width().min(780.0);
    let factor = (available / native.x).min(max_height / native.y).min(1.0);
    ui.add(egui::Image::new((texture.id(), native * factor)));
}

impl SetupApp {
    fn new(cc: &eframe::CreationContext<'_>) -> Self {
        cc.egui_ctx.set_visuals(egui::Visuals::dark());
        Self {
            source: std::env::current_dir()
                .map(|p| p.display().to_string())
                .unwrap_or_default(),
            destination: r"C:\Games\ScorchedPlanet".into(),
            language: Language::French,
            stage: Stage::Ready,
            progress: 0.0,
            status: String::new(),
            error: None,
            result: None,
            worker: None,
            banner: None,
            illustration: None,
            artwork_source: String::new(),
            artwork_status: String::new(),
            audit_status: String::new(),
            cd_language_source: String::new(),
            cd_languages: None,
            cd_language_note: String::new(),
        }
    }

    fn tr(&self, fr: &'static str, de: &'static str) -> &'static str {
        match self.language {
            Language::English | Language::French => fr,
            Language::German => de,
        }
    }

    /// Refresh once when the source path changes; the original SETUP.INI
    /// controls which game languages are available on that edition of the CD.
    fn refresh_cd_languages(&mut self) {
        if self.source == self.cd_language_source {
            return;
        }
        self.cd_language_source.clone_from(&self.source);
        self.cd_languages = None;
        match read_cd_languages(Path::new(self.source.trim())) {
            Ok(Some(available)) => {
                if !available.offers(self.language.code()) {
                    if let Some(code) = available.preferred_supported_code() {
                        if let Some(language) = Language::from_code(code) {
                            self.language = language;
                        }
                    }
                }
                self.cd_language_note = format!(
                    "Langues annoncees sur ce CD : anglais={}, francais={}, allemand={}, italien={}, espagnol={}.",
                    available.english, available.french, available.german,
                    available.italian, available.spanish,
                );
                self.cd_languages = Some(available);
            }
            Ok(None) => {
                self.cd_language_note =
                    "SETUP.INI introuvable : disponibilite des langues non verifiee.".into();
            }
            Err(error) => {
                self.cd_language_note = format!("SETUP.INI : {error}");
            }
        }
    }

    fn refresh_artwork(&mut self, ctx: &egui::Context) {
        if self.source == self.artwork_source {
            return;
        }
        self.artwork_source.clone_from(&self.source);
        self.banner = None;
        self.illustration = None;
        let root = Path::new(&self.source);
        if let Some(file) = original_art(root, &["BANNER.BMP", "COMPANY.BMP"]) {
            self.banner = load_texture(ctx, &file, "original-banner");
        }
        if let Some(file) = original_art(root, &["CPYRIGHT.BMP", "DIALOG.BMP"]) {
            self.illustration = load_texture(ctx, &file, "original-illustration");
        }
        self.artwork_status = if self.banner.is_none() && self.illustration.is_none() {
            "Les images InstallShield sont introuvables ou empaquetees dans les ressources historiques.".into()
        } else {
            "Illustrations chargees depuis la copie du CD original.".into()
        };
    }

    fn start(&mut self) {
        let options = InstallOptions {
            cd_root: PathBuf::from(self.source.trim()),
            destination: PathBuf::from(self.destination.trim()),
            language: self.language.code(),
        };
        let plan = match plan_install(&options) {
            Ok(plan) => plan,
            Err(error) => {
                self.error = Some(error);
                self.stage = Stage::Failed;
                return;
            }
        };

        let (sender, receiver) = mpsc::channel();
        self.worker = Some(receiver);
        self.stage = Stage::Installing;
        self.progress = 0.0;
        self.error = None;
        self.result = None;
        self.status = format!("{} fichiers a copier", plan.files.len());

        std::thread::spawn(move || {
            let result = execute_install(&options, &plan, |copied, total, file| {
                let _ = sender.send(WorkerMessage::Progress(
                    copied,
                    total,
                    file.to_owned(),
                ));
            });
            let _ = sender.send(WorkerMessage::Complete(result));
        });
    }

    fn update_worker(&mut self) {
        let events: Vec<_> = self
            .worker
            .as_ref()
            .map(|rx| rx.try_iter().collect())
            .unwrap_or_default();
        for event in events {
            match event {
                WorkerMessage::Progress(done, total, file) => {
                    self.progress = if total == 0 {
                        1.0
                    } else {
                        done as f32 / total as f32
                    };
                    self.status = file;
                }
                WorkerMessage::Complete(Ok(result)) => {
                    self.progress = 1.0;
                    self.stage = Stage::Complete;
                    self.result = Some(result);
                    self.worker = None;
                }
                WorkerMessage::Complete(Err(error)) => {
                    self.stage = Stage::Failed;
                    self.error = Some(error);
                    self.worker = None;
                }
            }
        }
    }
}

impl eframe::App for SetupApp {
    fn ui(&mut self, root_ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = root_ui.ctx().clone();
        self.update_worker();
        if self.stage == Stage::Installing {
            ctx.request_repaint_after(Duration::from_millis(120));
        }
        self.refresh_artwork(&ctx);
        self.refresh_cd_languages();

        egui::CentralPanel::default().show(root_ui, |ui| {
            ui.add_space(10.0);
            ui.heading("SCORCHED PLANET");
            ui.label(self.tr(
                "Installation moderne • Édition originale 1996",
                "Modernes Installationsprogramm • Originalausgabe 1996",
            ));
            ui.separator();

            if let Some(banner) = &self.banner {
                show_texture(ui, banner, 135.0);
            } else if let Some(art) = &self.illustration {
                show_texture(ui, art, 140.0);
            } else {
                egui::Frame::NONE.fill(Color32::from_rgb(31, 41, 55)).show(ui, |ui| {
                    ui.set_min_height(75.0);
                    ui.vertical_centered(|ui| {
                        ui.add_space(20.0);
                        ui.colored_label(
                            Color32::from_rgb(239, 195, 100),
                            "Criterion Studios • Virgin Interactive • 1996",
                        );
                    });
                });
            }
            ui.add_space(8.0);

            ui.label(self.tr(
                "Dossier contenant les fichiers copiés du CD :",
                "Ordner mit den vom Original-CD kopierten Dateien:",
            ));
            ui.horizontal(|ui| {
                ui.add_enabled(
                    self.stage != Stage::Installing,
                    egui::TextEdit::singleline(&mut self.source).desired_width(560.0),
                );
                if ui.add_enabled(
                    self.stage != Stage::Installing,
                    egui::Button::new(self.tr("Parcourir…", "Durchsuchen…")),
                ).clicked()
                    && let Some(folder) = rfd::FileDialog::new().pick_folder()
                {
                    self.source = folder.display().to_string();
                }
            });

            ui.add_space(8.0);
            ui.label(self.tr("Dossier d'installation :", "Installationsordner:"));
            ui.horizontal(|ui| {
                ui.add_enabled(
                    self.stage != Stage::Installing,
                    egui::TextEdit::singleline(&mut self.destination).desired_width(560.0),
                );
                if ui.add_enabled(
                    self.stage != Stage::Installing,
                    egui::Button::new(self.tr("Parcourir…", "Durchsuchen…")),
                ).clicked()
                    && let Some(folder) = rfd::FileDialog::new().pick_folder()
                {
                    self.destination = folder.display().to_string();
                }
            });

            ui.add_space(8.0);
            ui.horizontal(|ui| {
                ui.label(self.tr("Langue du jeu :", "Spielsprache:"));
                let can_english = self.cd_languages.as_ref().is_none_or(|languages| languages.english);
                let can_french = self.cd_languages.as_ref().is_none_or(|languages| languages.french);
                let can_german = self.cd_languages.as_ref().is_none_or(|languages| languages.german);
                ui.add_enabled_ui(self.stage != Stage::Installing, |ui| {
                    ui.add_enabled_ui(can_english, |ui| {
                        ui.radio_value(&mut self.language, Language::English, "English");
                    });
                    ui.add_enabled_ui(can_french, |ui| {
                        ui.radio_value(&mut self.language, Language::French, "Français");
                    });
                    ui.add_enabled_ui(can_german, |ui| {
                        ui.radio_value(&mut self.language, Language::German, "Deutsch");
                    });
                });
            });

            if !self.cd_language_note.is_empty() {
                ui.small(&self.cd_language_note);
            }
            ui.add_space(8.0);
            if ui.add_enabled(
                self.stage != Stage::Installing,
                egui::Button::new(self.tr("Analyser le contenu du CD…", "CD-Inhalt analysieren…")),
            ).clicked() {
                match audit_cd(Path::new(self.source.trim())) {
                    Ok(report) => {
                        if let Some(file) = rfd::FileDialog::new()
                            .set_file_name("ScorchedPlanet-CD-audit.txt")
                            .save_file()
                        {
                            self.audit_status = match std::fs::write(&file, report) {
                                Ok(()) => format!("Rapport enregistre : {}", file.display()),
                                Err(error) => format!("Impossible d'enregistrer le rapport : {error}"),
                            };
                        }
                    }
                    Err(error) => {
                        self.audit_status = format!("Analyse impossible : {error}");
                    }
                }
            }
            if !self.audit_status.is_empty() {
                ui.small(&self.audit_status);
            }

            ui.add_space(8.0);
            ui.small(self.tr(
                "Les fichiers du CD ne seront pas modifiés. Aucune image commerciale n'est intégrée au programme.",
                "Die CD-Dateien bleiben unverändert. Das Programm enthält keine kommerziellen Grafiken.",
            ));
            ui.separator();

            match self.stage {
                Stage::Ready => {
                    if ui.button(self.tr("Installer Scorched Planet", "Scorched Planet installieren")).clicked() {
                        self.start();
                    }
                }
                Stage::Installing => {
                    ui.heading(self.tr("Installation en cours", "Installation läuft"));
                    ui.add(egui::ProgressBar::new(self.progress.clamp(0.0, 1.0))
                        .show_percentage()
                        .desired_width(ui.available_width()));
                    ui.small(&self.status);
                }
                Stage::Complete => {
                    ui.colored_label(Color32::LIGHT_GREEN,
                        self.tr("Copie terminée.", "Kopieren abgeschlossen."));
                    if let Some(result) = &self.result {
                        ui.label(format!("{} fichiers • {} Mio",
                            result.files_copied, result.total_bytes / (1024 * 1024)));
                        ui.label(result.destination.display().to_string());
                        for warning in &result.warnings {
                            ui.colored_label(Color32::YELLOW, warning);
                        }
                    }
                    ui.small(self.tr(
                        "La compatibilité 3dfx et l'éventuel contrôle du CD au lancement restent à corriger.",
                        "3dfx-Kompatibilität und mögliche CD-Abfragen beim Start werden später behoben.",
                    ));
                }
                Stage::Failed => {
                    ui.colored_label(Color32::LIGHT_RED, self.tr(
                        "Installation interrompue", "Installation fehlgeschlagen",
                    ));
                    if let Some(error) = &self.error {
                        ui.label(error);
                    }
                    if ui.button(self.tr("Réessayer", "Erneut versuchen")).clicked() {
                        self.stage = Stage::Ready;
                        self.error = None;
                    }
                }
            }

            ui.add_space(8.0);
            ui.small(&self.artwork_status);
            ui.small("Scorched Planet Setup 0.1 • development preview");
        });
    }
}

fn main() -> eframe::Result {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([850.0, 665.0])
            .with_min_inner_size([710.0, 540.0]),
        ..Default::default()
    };
    eframe::run_native(
        "Scorched Planet Setup",
        options,
        Box::new(|cc| Ok(Box::new(SetupApp::new(cc)))),
    )
}
