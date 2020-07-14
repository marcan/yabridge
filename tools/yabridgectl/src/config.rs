// yabridge: a Wine VST bridge
// Copyright (C) 2020  Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

use rayon::prelude::*;
use serde_derive::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Display;
use std::fs;
use std::path::{Path, PathBuf};
use xdg::BaseDirectories;

use crate::files::{self, SearchResults};

/// The name of the config file, relative to `$XDG_CONFIG_HOME/CONFIG_PREFIX`.
const CONFIG_FILE_NAME: &str = "config.toml";
/// The name of the XDG base directory prefix for yabridgectl, relative to `$XDG_CONFIG_HOME` and
/// `$XDG_DATA_HOME`.
const YABRIDGECTL_PREFIX: &str = "yabridgectl";

/// The name of the library file we're searching for.
const LIBYABRIDGE_NAME: &str = "libyabridge.so";
/// The name of the XDG base directory prefix for yabridge's own files, relative to
/// `$XDG_CONFIG_HOME` and `$XDG_DATA_HOME`.
const YABRIDGE_PREFIX: &str = "yabridge";

/// The configuration used for yabridgectl. This will be serialized to and deserialized from
/// `$XDG_CONFIG_HOME/yabridge/config.toml`.
#[derive(Deserialize, Serialize, Debug)]
pub struct Config {
    /// The installation method to use. We will default to creating copies since that works
    /// everywehre.
    pub method: InstallationMethod,
    /// The path to the directory containing `libyabridge.so`. If not set, then yabridgectl will
    /// look in `/usr/lib` and `$XDG_DATA_HOME/yabridge` since those are the expected locations for
    /// yabridge to be installed in.
    pub yabridge_home: Option<PathBuf>,
    /// Directories to search for Windows VST plugins. We're using an ordered set here out of
    /// convenience so we can't get duplicates and the config file is always sorted.
    pub plugin_dirs: BTreeSet<PathBuf>,
}

/// Specifies how yabridge will be set up for the found plugins.
#[derive(Deserialize, Serialize, Debug)]
#[serde(rename_all = "snake_case")]
pub enum InstallationMethod {
    /// Create a copy of `libyabridge.so` for every Windows VST2 plugin .dll file found. After
    /// updating yabridge, the user will have to rerun `yabridgectl sync` to copy over the new
    /// version.
    Copy,
    /// This will create a symlink to `libyabridge.so` for every VST2 .dll file in the plugin
    /// directories. As explained in the readme, this makes updating easier and remvoes the need to
    /// modify the `PATH` environment variable.
    Symlink,
}

impl Display for InstallationMethod {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self {
            InstallationMethod::Copy => write!(f, "copy"),
            InstallationMethod::Symlink => write!(f, "symlink"),
        }
    }
}

impl Config {
    /// Try to read the config file, creating a new default file if necessary. This will fail if the
    /// file could not be created or if it could not be parsed.
    pub fn read() -> Result<Config, String> {
        match yabridgectl_directories()?.find_config_file(CONFIG_FILE_NAME) {
            Some(path) => {
                let toml_str = fs::read_to_string(&path).map_err(|err| {
                    format!(
                        "Could not read config file at '{}': {}",
                        path.display(),
                        err
                    )
                })?;

                Ok(toml::from_str(&toml_str)
                    .map_err(|err| format!("Could not parse TOML: {}", err))?)
            }
            None => {
                let defaults = Config {
                    method: InstallationMethod::Copy,
                    yabridge_home: None,
                    plugin_dirs: BTreeSet::new(),
                };

                // If no existing config file exists, then write a new config file with default
                // values
                defaults.write()?;

                Ok(defaults)
            }
        }
    }

    /// Write the config to disk, creating the file if it does not yet exist.
    pub fn write(&self) -> Result<(), String> {
        let toml_str = toml::to_string_pretty(&self)
            .map_err(|err| format!("Could not format TOML: {}", err))?;
        let config_file = yabridgectl_directories()?
            .place_config_file(CONFIG_FILE_NAME)
            .map_err(|err| format!("Could not write config file: {}", err))?;

        fs::write(&config_file, toml_str).map_err(|err| {
            format!(
                "Could not write config file to '{}': {}",
                config_file.display(),
                err
            )
        })
    }

    /// Return the path to `libyabridge.so`, or a descriptive error if it can't be found. If
    /// `yabridge_home` is `None`, then we'll search in both `/usr/lib` and
    /// `$XDG_DATA_HOME/yabridge`.
    pub fn libyabridge(&self) -> Result<PathBuf, String> {
        match &self.yabridge_home {
            Some(directory) => {
                let candidate = directory.join(LIBYABRIDGE_NAME);
                if candidate.exists() {
                    Ok(candidate)
                } else {
                    Err(format!(
                        "Could not find {} in '{}'.",
                        LIBYABRIDGE_NAME,
                        directory.display()
                    ))
                }
            }
            None => {
                // Search in the two common installation locations if no path was set explicitely
                let system_path = Path::new("/usr/lib");
                let user_path = yabridge_directories()?.get_data_home();
                for directory in &[system_path, &user_path] {
                    let candidate = directory.join(LIBYABRIDGE_NAME);
                    if candidate.exists() {
                        return Ok(candidate);
                    }
                }

                Err(format!(
                    "Could not find {} in either '{}' or '{}'. You can tell yabridgectl where \
                     to search for it using 'yabridgectl set --path=<path>'.",
                    LIBYABRIDGE_NAME,
                    system_path.display(),
                    user_path.display()
                ))
            }
        }
    }

    /// Search for VST2 plugins in all of the registered plugins directories. This will return an
    /// error if `winedump` could not be called.
    pub fn index_directories(&self) -> Result<BTreeMap<&Path, SearchResults>, std::io::Error> {
        self.plugin_dirs
            .par_iter()
            .map(|path| files::index(path).map(|search_results| (path.as_path(), search_results)))
            .collect()
    }
}

/// Fetch the XDG base directories for yabridge's own files, converting any error messages if this
/// somehow fails into a printable string to reduce boiler plate. This is only used when searching
/// for `libyabridge.so` when no explicit search path has been set.
fn yabridge_directories() -> Result<BaseDirectories, String> {
    BaseDirectories::with_prefix(YABRIDGE_PREFIX)
        .map_err(|err| format!("Error while parsing base directories: {}", err))
}

/// Fetch the XDG base directories used for yabridgectl, converting any error messages if this
/// somehow fails into a printable string to reduce boiler plate.
fn yabridgectl_directories() -> Result<BaseDirectories, String> {
    BaseDirectories::with_prefix(YABRIDGECTL_PREFIX)
        .map_err(|err| format!("Error while parsing base directories: {}", err))
}
