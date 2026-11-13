// SPDX-License-Identifier: GPL-3.0-or-later
//! Installer/launcher diagnostic log beside the patcher executable.
//! The GameFrame DLL writes separately to ScorchedPlanetFix.log.
//! Hardware inventory reuses CrimsonSkiesFix GPL-3.0-or-later routines.
//! No usernames, serial numbers, MAC addresses or raw registry paths logged.
use std::fs::{File,OpenOptions};
use std::io::Write;
use std::path::{Path,PathBuf};
use std::sync::{Arc,Mutex};
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
        let file=OpenOptions::new().create(true).append(true).open(&path)
            .map_err(|e|format!("Unable to create log file {}: {e}",path.display()))?;
        let result=Self{handle:Arc::new(Mutex::new(file)),path,session_id:new_session_id()};
        let now=SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default();
        result.write("---------------------------------------------");
        result.write(&format!("ScorchedPlanetFix {version} - session start UTC unix_time={} session_id={}",now.as_secs(),result.session_id));
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
            if let Ok(mut file)=OpenOptions::new().create(true).append(true).open(&path){
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
    fn installer_log_is_separate_and_has_session_id(){
        let root=std::env::temp_dir().join(format!("scorched-log-{}",new_session_id()));
        std::fs::create_dir(&root).unwrap();
        let logger=Logger::new(&root,"2.0.1-test").unwrap();
        assert_eq!(logger.path,root.join("ScorchedPlanetFix_installer.log"));
        assert!(!root.join("ScorchedPlanetFix.log").exists());
        assert!(!logger.session_id().is_empty());
        let content=std::fs::read_to_string(&logger.path).unwrap();
        assert!(content.contains(&format!("session_id={}",logger.session_id())));
        drop(logger);
        std::fs::remove_dir_all(root).unwrap();
    }
}
