project "Ignite.Managed.Test"
    location "%{wks.location}/ignite/managed.test"
    kind "ConsoleApp"
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
        "Ignite.Managed",
        "Ignite.ScriptEngine"
    }

    dependson {
        "Ignite.Engine"
    }

    nuget {
        "NUnit:4.6.1"
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
            ApplicationManifest = "app.manifest",
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
