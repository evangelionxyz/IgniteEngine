// Copyright (c) 2026 Evangelion Manuhutu

pub fn ends_with(value: &str, ending: &str) -> bool {
    value.ends_with(ending)
}

pub fn to_lower(str: &str) -> String {
    str.to_lowercase()
}

pub fn trim(str: &str) -> &str {
    str.trim_matches(|c: char| c == ' ' || c == '\t')
}

pub fn split_string<'a>(str: &'a str, delimiter: char) -> Vec<&'a str> {
    str.split(delimiter).collect()
}

pub fn replace_with(out_str: &str, target_key: &str, replace_key: &str) -> String {
    out_str.replace(target_key, replace_key)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_string_utils() {
        assert!(ends_with("hello.png", ".png"));
        assert!(!ends_with("hello.png", ".jpg"));
        assert_eq!(to_lower("IGNITE Engine"), "ignite engine");
        assert_eq!(trim("  hello world \t"), "hello world");
        assert_eq!(split_string("a,b,c", ','), vec!["a", "b", "c"]);
        assert_eq!(replace_with("foo_bar_foo", "foo", "baz"), "baz_bar_baz");
    }
}
