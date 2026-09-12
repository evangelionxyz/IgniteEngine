project "Ignite.Managed"
    location "%{wks.location}/ignite/managed"
    kind "SharedLib"
    language "C#"
    dotnetframework "net10.0"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{prj.location}/**.cs"
    }

    removefiles {
        "%{prj.location}/bin/**",
        "%{prj.location}/obj/**"
    }

    links {
    }

    dependson {
        "Ignite.Engine"
    }

    nuget {
    }

    filter { "action:vs* or system:windows" }
        vsprops {
            AppendTargetFrameworkToOutputPath = "false",
            DebugType = "pdbonly",
            Nullable = "enable",
            AllowUnsafeBlocks = "true",
            CopyLocalLockFileAssemblies = "true",
            EnableDynamicLoading = "true",
            ImplicitUsing = "enable",
            EnableNativeCodeDebugging = "true",
            PlatformTarget = "x64"
        }

    filter "configurations:Debug or Debug-Profiling"
        symbols "on"
        optimize "off"

    filter "configurations:Release or Release-Profiling"
        optimize "on"
        symbols "off"

    filter "configurations:Shipping or Shipping-Profiling"
        optimize "on"
        symbols "off"
