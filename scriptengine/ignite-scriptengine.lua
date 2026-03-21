project "IgniteScriptEngine"
    location "%{wks.location}/scriptengine"
    kind "SharedLib"
    language "C#"
    dotnetframework "net9.0"
    
    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{prj.location}/Core/**.cs",
        "%{prj.location}/Math/**.cs",
        "%{prj.location}/Properties/**.cs"
    }

    links {
        "MochiSharp.Managed"
    }

    filter { "action:vs* or system:windows" }
        vsprops {
            AppendTargetFrameworkToOutputPath = "false",
            Nullable = "enable",
            CopyLocalLockFileAssemblies = "true",
            EnableDynamicLoading = "true",
            ImplicitUsing = "enable"
        }
        
    filter "configurations:Debug"
        symbols "on"
        optimize "off"

    filter "configurations:Release"
        optimize "on"
        symbols "off"
    
    filter "configurations:Shipping"
        optimize "on"
        symbols "off"