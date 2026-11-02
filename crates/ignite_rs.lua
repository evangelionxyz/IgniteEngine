project "Ignite.Rust"
    location "%{wks.location}/crates"
    kind "Makefile"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)
    
    files {
        "%{wks.location}/crates/**.rs",
        "%{wks.location}/crates/**.toml",


    }

    removefiles {
        "%{wks.location}/crates/target/**",
    }

    filter "configurations:Debug*"
        buildcommands {
            "cargo build --manifest-path \"%{wks.location}/crates/Cargo.toml\""
        }
        cleancommands {
            "cargo clean --manifest-path \"%{wks.location}/crates/Cargo.toml\""
        }
        rebuildcommands {
            "cargo clean --manifest-path \"%{wks.location}/crates/Cargo.toml\" && cargo build --manifest-path \"%{wks.location}/crates/Cargo.toml\""
        }

    filter "configurations:Release*"
        buildcommands {
            "cargo build --manifest-path \"%{wks.location}/crates/Cargo.toml\" --release"
        }
        cleancommands {
            "cargo clean --manifest-path \"%{wks.location}/crates/Cargo.toml\""
        }
        rebuildcommands {
            "cargo clean --manifest-path \"%{wks.location}/crates/Cargo.toml\" && cargo build --manifest-path \"%{wks.location}/crates/Cargo.toml\" --release"
        }

    filter "configurations:Shipping*"
        buildcommands {
            "cargo build --manifest-path \"%{wks.location}/crates/Cargo.toml\" --release"
        }
        cleancommands {
            "cargo clean --manifest-path \"%{wks.location}/crates/Cargo.toml\""
        }
        rebuildcommands {
            "cargo clean --manifest-path \"%{wks.location}/crates/Cargo.toml\" && cargo build --manifest-path \"%{wks.location}/crates/Cargo.toml\" --release"
        }
