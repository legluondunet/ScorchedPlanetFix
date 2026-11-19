// SPDX-License-Identifier: GPL-3.0-or-later
//! Installer/launcher diagnostic log beside the patcher executable.
//! The GameFrame DLL writes separately to ScorchedPlanetFix.log.
//! Hardware inventory reuses CrimsonSkiesFix GPL-3.0-or-later routines.
//! No usernames, serial numbers, MAC addresses or raw registry paths logged.
use std::fs::{File,OpenOptions};
use std::io::Write;
use std::path::{Path,PathBuf};
use std::sync::{Arc,Mutex};
use std::sync::atomic::{AtomicBool,Ordering};

// The executable creates at most one log session. An error that happens before
// Logger::new (notably an invalid CLI argument) must also replace prior runs.
static SESSION_LOG_OPENED: AtomicBool = AtomicBool::new(false);
use std::time::{SystemTime,UNIX_EPOCH};

/// A unique, filesystem-safe identifier reused in the launcher and DLL logs.
pub fn new_session_id()->String{
    let now=SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default();
    format!("{}-{}",now.as_nanos(),std::process::id())
}

#[derive(Clone)]
pub struct Logger {
    handle:Arc<Mutex<File>>,
    pub path:PathBuf,
    session_id:String,
}
impl Logger {
    pub fn new(root:&Path,version:&str)->Result<Self,String>{
        let path=root.join("ScorchedPlanetFix_installer.log");
        let file=OpenOptions::new().create(true).write(true).truncate(true).open(&path)
            .map_err(|e|format!("Unable to create log file {}: {e}",path.display()))?;
        let result=Self{handle:Arc::new(Mutex::new(file)),path,session_id:new_session_id()};
        let now=SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default();
        result.write("---------------------------------------------");
        result.write(&format!("ScorchedPlanetFix {version} - session start UTC unix_time={} session_id={}",now.as_secs(),result.session_id));
        SESSION_LOG_OPENED.store(true, Ordering::Release);
        Ok(result)
    }
    pub fn session_id(&self)->&str{ &self.session_id }
    pub fn write(&self,message:&str){
        if let Ok(mut file)=self.handle.lock(){
            // Forbid log injection through filesystem names or source inputs.
            let text=message.replace(['\r','\n']," ");
            let _=writeln!(file,"{text}");
            let _=file.flush();
        }
    }
    /// Record one final error, including failures before the main logger
    /// was initialized. Prefer the EXE's directory and fall back to cwd.
    /// If both locations are unwritable, stderr still carries the error.
    pub fn emergency_error(root:Option<&Path>,message:&str){
        let cwd=std::env::current_dir().ok();
        let candidates=[root,cwd.as_deref()];
        for directory in candidates.into_iter().flatten(){
            let path=directory.join("ScorchedPlanetFix_installer.log");
            // Append only if this process has already started its own session.
            // For startup argument errors, discard stale log lines from a
            // previous invocation instead of manufacturing a cumulative log.
            let mut options=OpenOptions::new();
            options.create(true).write(true);
            if SESSION_LOG_OPENED.load(Ordering::Acquire) {
                options.append(true);
            } else {
                options.truncate(true);
            }
            if let Ok(mut file)=options.open(&path){
                let safe=message.replace(['\r','\n']," ");
                let _=writeln!(file,"ERROR: {safe}");
                let _=file.flush();
                return;
            }
        }
    }
    pub fn system_information(&self) {
        self.write("SYSTEM: passive Windows / Wine hardware inventory (no personal identifiers)");
        crate::diagnostics::report(|line|self.write(line));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn each_new_launcher_session_truncates_previous_log() {
        let root=std::env::temp_dir().join(format!("scorched-log-reset-{}",new_session_id()));
        std::fs::create_dir(&root).unwrap();
        let first=Logger::new(&root,"test-first").unwrap();
        let old_session=first.session_id().to_owned();
        first.write("OLD_SESSION_SHOULD_NOT_BE_PRESENT");
        drop(first);
        let second=Logger::new(&root,"test-second").unwrap();
        let content=std::fs::read_to_string(&second.path).unwrap();
        assert!(content.contains("ScorchedPlanetFix test-second"), "{content}");
        assert!(content.contains(&format!("session_id={}",second.session_id())));
        assert!(!content.contains(&old_session));
        assert!(!content.contains("OLD_SESSION_SHOULD_NOT_BE_PRESENT"));
        assert_eq!(content.matches("session start UTC").count(),1);
        drop(second);
        std::fs::remove_dir_all(root).unwrap();
    }
    #[test]
    fn installer_log_is_separate_and_has_session_id(){
        let root=std::env::temp_dir().join(format!("scorched-log-{}",new_session_id()));
        std::fs::create_dir(&root).unwrap();
        let logger=Logger::new(&root,env!("CARGO_PKG_VERSION")).unwrap();
        assert_eq!(logger.path,root.join("ScorchedPlanetFix_installer.log"));
        assert!(!root.join("ScorchedPlanetFix.log").exists());
        assert!(!logger.session_id().is_empty());
        let content=std::fs::read_to_string(&logger.path).unwrap();
        assert!(content.contains(&format!("session_id={}",logger.session_id())));
        drop(logger);
        std::fs::remove_dir_all(root).unwrap();
    }
}
