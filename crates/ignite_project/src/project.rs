// Copyright (c) 2026 Evangelion Manuhutu

use std::path::{Path, PathBuf};
use serde::{Serialize, Deserialize};
use ignite_asset::AssetHandle;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProjectConfig {
    pub name: String,
    pub asset_directory: String,
    pub start_scene: AssetHandle,
}

impl Default for ProjectConfig {
    fn default() -> Self {
        Self {
            name: "Untitled".to_string(),
            asset_directory: "Assets".to_string(),
            start_scene: AssetHandle::NULL,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Project {
    pub config: ProjectConfig,
    pub project_directory: PathBuf,
}

impl Project {
    pub fn new(name: impl Into<String>, project_dir: impl AsRef<Path>) -> Self {
        Self {
            config: ProjectConfig {
                name: name.into(),
                ..Default::default()
            },
            project_directory: project_dir.as_ref().to_path_buf(),
        }
    }

    pub fn get_asset_directory_path(&self) -> PathBuf {
        self.project_directory.join(&self.config.asset_directory)
    }

    pub fn resolve_asset_path(&self, relative_path: impl AsRef<Path>) -> PathBuf {
        self.get_asset_directory_path().join(relative_path)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_project_paths() {
        let proj = Project::new("MyGame", "D:/Games/MyGame");
        assert_eq!(proj.config.name, "MyGame");
        assert_eq!(proj.get_asset_directory_path(), PathBuf::from("D:/Games/MyGame/Assets"));
        assert_eq!(proj.resolve_asset_path("Textures/hero.png"), PathBuf::from("D:/Games/MyGame/Assets/Textures/hero.png"));
    }
}
