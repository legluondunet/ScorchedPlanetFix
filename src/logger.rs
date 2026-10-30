// SPDX-License-Identifier: GPL-3.0-or-later
//! User-visible diagnostic log, in the exact directory of the patcher.
//! Hardware inventory reuses CrimsonSkiesFix GPL-3.0-or-later routines.
//! No usernames, serial numbers, MAC addresses or raw registry paths logged.
use std::fs::{File,OpenOptions};
use std::io::Write;
use std::path::{Path,PathBuf};
use std::sync::{Arc,Mutex};
use std::time::{SystemTime,UNIX_EPOCH};

#[derive(Clone)]
pub struct Logger {
    handle:Arc<Mutex<File>>,
    pub path:PathBuf,
}
impl Logger {
    pub fn new(root:&Path,version:&str)->Result<Self,String>{
        let path=root.join("ScorchedPlanetFix.log");
        let file=OpenOptions::new().create(true).append(true).open(&path)
            .map_err(|e|format!("Impossible de créer le log {}: {e}",path.display()))?;
        let result=Self{handle:Arc::new(Mutex::new(file)),path};
        let now=SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default();
        result.write("---------------------------------------------");
        result.write(&format!("ScorchedPlanetFix {version} - session start UTC unix_time={}",now.as_secs()));
        Ok(result)
    }
    pub fn write(&self,message:&str){
        if let Ok(mut file)=self.handle.lock(){
            // Forbid log injection through filesystem names or source inputs.
            let text=message.replace(['\r','\n']," ");
            let _=writeln!(file,"{text}");
            let _=file.flush();
        }
    }
    pub fn error(&self,message:&str){
        self.write(&format!("ERROR: {message}"));
    }
    /// Fallback for an error that occurs before the regular logger is open.
    /// Prefer the patcher's directory, then the current working directory.
    /// This cannot write a log if all destinations are unwritable.
    pub fn emergency_error(root:Option<&Path>,message:&str){
        let cwd=std::env::current_dir().ok();
        let candidates=[root,cwd.as_deref()];
        for directory in candidates.into_iter().flatten(){
            let path=directory.join("ScorchedPlanetFix.log");
            if let Ok(mut file)=OpenOptions::new().create(true).append(true).open(&path){
                let safe=message.replace(['\r','\n']," ");
                let _=writeln!(file,"ERROR (startup): {safe}");
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
