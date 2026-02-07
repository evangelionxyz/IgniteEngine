project "IgniteScriptEngine"
    location "%{wks.location}/scriptengine"
    kind "SharedLib"
    language "C#"
    dotnetframework "net9.0"
    
    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "Core/**.cs",
        "Math/**.cs",
        "Properties/**.cs"
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
        vsprops {
            OutputPath = "..\\bin\\Debug\\",
            IntermediateOutputPath = "..\\bin\\objs\\Debug\\IgniteScriptEngine\\"
        }

    filter "configurations:Release"
        optimize "on"
        symbols "off"
        vsprops {
            OutputPath = "..\\bin\\Release\\",
            IntermediateOutputPath = "..\\bin\\objs\\Release\\IgniteScriptEngine\\"
        }
    
    filter "configurations:Shipping"
        vsprops {
            OutputPath = "..\\bin\\Shipping\\",
            IntermediateOutputPath = "..\\bin\\objs\\Shipping\\IgniteScriptEngine\\"
        }