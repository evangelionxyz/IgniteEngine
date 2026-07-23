// Copyright (c) 2026 Evangelion Manuhutu

use serde::{Serialize, Deserialize};
use crate::EntityId;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct ScriptComponent {
    pub class_name: String,
    pub script_handle: u64, // MochiSharp script instance handle
}

impl ScriptComponent {
    pub fn new(class_name: impl Into<String>) -> Self {
        Self {
            class_name: class_name.into(),
            script_handle: 0,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct HierarchyComponent {
    pub parent: Option<EntityId>,
    pub children: Vec<EntityId>,
}

impl HierarchyComponent {
    pub fn new() -> Self {
        Self::default()
    }
}
