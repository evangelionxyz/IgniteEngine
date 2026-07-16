project "Ignite.ScriptEngine"
    location "%{wks.location}/scriptengine"
    kind "SharedLib"
    language "C#"
    dotnetframework "net10.0"
    
    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{prj.location}/Ignite/**.cs",
        "%{prj.location}/Properties/**.cs"
    }

    links {
        "MochiSharp.Managed"
    }

    filter { "action:vs* or system:windows" }
        vsprops {
            AppendTargetFrameworkToOutputPath = "false",
            DebugType = "pdbonly",
            Nullable = "enable",
            AllowUnsafeBlocks = "true",
            CopyLocalLockFileAssemblies = "true",
            EnableDynamicLoading = "true",
            ImplicitUsing = "enable"
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