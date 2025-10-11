project "IgniteScript"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.8"
    
    targetdir (OUTPUT_DIR .. "/lib/")
    objdir (INTOUTPUT_DIR)

    files {
        "src/**.cs",
        "properties/**.cs",
        "**.cs",
    }

    filter "configurations:Debug"
        optimize "On"
        symbols "Default"

    filter "configurations:Release"
        optimize "Full"
        symbols "Default"

    filter "configurations:Shipping"
        optimize "Full"
        symbols "Off"