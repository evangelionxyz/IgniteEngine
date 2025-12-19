project "IgniteScriptEngine"
    location "%{wks.location}/scriptengine"
    kind "SharedLib"
    language "C#"
    dotnetframework "net9.0"
    
    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "**.cs"
    }

    links {
        "MochiSharp.Managed"
    }

    filter { "action:vs* or system:windows" }
        vsprops {
            AppendTargetFrameworkToOutputPath = "false",
            Nullable = "disable",
            CopyLocalLockFileAssemblies = "true",
            EnableDynamicLoading = "true",
            ImplicitUsing = "enable"
        }
        
    filter "configurations:Debug"
        symbols "on"

    filter "configurations:Release"
        optimize "on"
        symbols "off"