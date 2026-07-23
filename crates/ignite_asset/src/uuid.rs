// Copyright (c) 2026 Evangelion Manuhutu

use rand::RngExt;
use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub struct UUID(pub u64);

impl UUID {
    pub const NULL: UUID = UUID(0);

    pub fn new() -> Self {
        Self(rand::rng().random())
    }

    pub fn from_u64(value: u64) -> Self {
        Self(value)
    }

    pub fn is_valid(&self) -> bool {
        self.0 != 0
    }

    pub const fn value(self) -> u64 {
        self.0
    }
}

impl Default for UUID {
    fn default() -> Self {
        UUID::NULL
    }
}

impl From<u64> for UUID {
    fn from(value: u64) -> Self {
        Self(value)
    }
}

impl From<UUID> for u64 {
    fn from(uuid: UUID) -> Self {
        uuid.0
    }
}

impl fmt::Display for UUID {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}
