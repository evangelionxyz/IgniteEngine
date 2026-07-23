// Copyright (c) 2026 Evangelion Manuhutu

use serde::{Serialize, Deserialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
#[repr(transparent)]
pub struct EntityId(pub u64);

impl EntityId {
    pub const NULL: EntityId = EntityId(0);

    pub fn is_valid(&self) -> bool {
        self.0 != 0
    }
}

impl Default for EntityId {
    fn default() -> Self {
        Self::NULL
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TagComponent {
    pub name: String,
}

impl TagComponent {
    pub fn new(name: impl Into<String>) -> Self {
        Self { name: name.into() }
    }
}

impl Default for TagComponent {
    fn default() -> Self {
        Self {
            name: "Entity".to_string(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
#[repr(C)]
pub struct TransformComponent {
    pub translation: [f32; 3],
    pub rotation: [f32; 3], // Euler angles in radians
    pub scale: [f32; 3],
}

impl TransformComponent {
    pub fn new(translation: [f32; 3], rotation: [f32; 3], scale: [f32; 3]) -> Self {
        Self {
            translation,
            rotation,
            scale,
        }
    }
}

impl Default for TransformComponent {
    fn default() -> Self {
        Self {
            translation: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0],
            scale: [1.0, 1.0, 1.0],
        }
    }
}
