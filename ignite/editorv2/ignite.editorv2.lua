project "Ignite.EditorV2"
    location "%{wks.location}/ignite/editorv2"
    kind "WindowedApp"
    language "C#"
    dotnetframework "net10.0"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{prj.location}/**.cs",
        "%{prj.location}/**.axaml",
        "%{prj.location}/app.manifest"
    }

    removefiles {
        "%{prj.location}/bin/**",
        "%{prj.location}/obj/**"
    }

    links {
        "Ignite.ScriptEngine"
    }

    dependson {
        "Ignite.Engine"
    }

    nuget {
        "Avalonia:12.1.2",
        "Avalonia.Desktop:12.1.2",
        "Avalonia.Themes.Fluent:12.1.2",
        "Avalonia.Fonts.Inter:12.1.2",
        "AvaloniaUI.DiagnosticsSupport:2.2.3",
        "Dock.Avalonia:11.2.0.2",
        "Dock.Model.Mvvm:11.2.0.2",
        "CommunityToolkit.Mvvm:8.4.0",
        "Avalonia.Svg.Skia:11.3.0"
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
            ApplicationManifest = "app.manifest"
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
