// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::HashMap;
use crate::{EntityId, TagComponent, TransformComponent, ScriptComponent, HierarchyComponent};

#[derive(Debug, Default)]
pub struct Scene {
    pub name: String,
    next_entity_id: u64,
    entities: Vec<EntityId>,
    tags: HashMap<EntityId, TagComponent>,
    transforms: HashMap<EntityId, TransformComponent>,
    scripts: HashMap<EntityId, ScriptComponent>,
    hierarchies: HashMap<EntityId, HierarchyComponent>,
}

impl Scene {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            next_entity_id: 1,
            ..Default::default()
        }
    }

    pub fn create_entity(&mut self, name: impl Into<String>) -> EntityId {
        let id = EntityId(self.next_entity_id);
        self.next_entity_id += 1;

        self.entities.push(id);
        self.tags.insert(id, TagComponent::new(name));
        self.transforms.insert(id, TransformComponent::default());
        self.hierarchies.insert(id, HierarchyComponent::default());
        id
    }

    pub fn destroy_entity(&mut self, id: EntityId) -> bool {
        if !id.is_valid() || !self.entities.contains(&id) {
            return false;
        }

        self.entities.retain(|&e| e != id);
        self.tags.remove(&id);
        self.transforms.remove(&id);
        self.scripts.remove(&id);
        self.hierarchies.remove(&id);
        true
    }

    pub fn get_tag(&self, id: EntityId) -> Option<&TagComponent> {
        self.tags.get(&id)
    }

    pub fn set_tag(&mut self, id: EntityId, name: impl Into<String>) -> bool {
        if let Some(tag) = self.tags.get_mut(&id) {
            tag.name = name.into();
            true
        } else {
            false
        }
    }

    pub fn get_transform(&self, id: EntityId) -> Option<&TransformComponent> {
        self.transforms.get(&id)
    }

    pub fn set_transform(&mut self, id: EntityId, transform: TransformComponent) -> bool {
        if self.entities.contains(&id) {
            self.transforms.insert(id, transform);
            true
        } else {
            false
        }
    }

    pub fn add_script(&mut self, id: EntityId, script: ScriptComponent) -> bool {
        if self.entities.contains(&id) {
            self.scripts.insert(id, script);
            true
        } else {
            false
        }
    }

    pub fn get_script(&self, id: EntityId) -> Option<&ScriptComponent> {
        self.scripts.get(&id)
    }

    pub fn entity_count(&self) -> usize {
        self.entities.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scene_entity_lifecycle() {
        let mut scene = Scene::new("TestScene");
        let player = scene.create_entity("Player");
        assert!(player.is_valid());
        assert_eq!(scene.entity_count(), 1);

        assert_eq!(scene.get_tag(player).unwrap().name, "Player");

        let mut transform = TransformComponent::default();
        transform.translation = [10.0, 5.0, 0.0];
        scene.set_transform(player, transform);

        assert_eq!(scene.get_transform(player).unwrap().translation, [10.0, 5.0, 0.0]);

        assert!(scene.destroy_entity(player));
        assert_eq!(scene.entity_count(), 0);
    }
}
