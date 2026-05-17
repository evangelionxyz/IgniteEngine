project "MochiSharp.Managed"
    location "%{THIRDPARTY_DIR}/MochiSharp/MochiSharp.Managed"
    kind "SharedLib"
    language "C#"
    dotnetframework "net10.0"
    vsprops {
        AppendTargetFrameworkToOutputPath = "false",
        Nullable = "enable",
        AllowUnsafeBlocks = "true",
        CopyLocalLockFileAssemblies = "true",
        EnableDynamicLoading = "true",
        ImplicitUsing = "enable"
    }

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{prj.location}/**.cs",
    }
