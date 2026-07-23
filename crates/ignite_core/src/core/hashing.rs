// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

pub fn hash_combine(seed: &mut u64, hash_val: u64) {
    const MAGIC_NUMBER: u64 = 0x9e3779b9;
    *seed ^= hash_val
        .wrapping_add(MAGIC_NUMBER)
        .wrapping_add(*seed << 6)
        .wrapping_add(*seed >> 2);
}

pub fn hash_value<T: Hash>(value: &T) -> u64 {
    let mut hasher = DefaultHasher::new();
    value.hash(&mut hasher);
    hasher.finish()
}

pub fn hash_str(str_val: &str) -> u64 {
    hash_value(&str_val)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hashing() {
        let mut seed = 0u64;
        hash_combine(&mut seed, 12345);
        assert_ne!(seed, 0);

        let h1 = hash_str("Ignite");
        let h2 = hash_str("Ignite");
        let h3 = hash_str("Engine");
        assert_eq!(h1, h2);
        assert_ne!(h1, h3);
    }
}
