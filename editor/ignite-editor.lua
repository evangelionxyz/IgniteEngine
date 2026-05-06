project "IgniteEditor"
    location "%{wks.location}/editor"
    kind "ConsoleApp"
    staticruntime "off"
    architecture "x64"
    language "c++"
    cppdialect "c++23"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "src/**.cpp",
        "src/**.hpp",
        "src/**.h",
        "../resources/qt_schema/**.ui",
    }

    links {
        "IgniteEngine",
        "Qt6Core",
        "Qt6Gui",
        "Qt6UiTools",
        "Qt6Widgets",
        "JOLT",
        "ZLIB",
        "YAMLCPP"
    }

    includedirs {
        "src",
        "%{wks.location}/engine/src",
        "%{IncludeDir.SDL3}",
        "%{LibraryDir.QT}/include",
        "%{LibraryDir.QT}/include/QtCore",
        "%{LibraryDir.QT}/include/QtGui",
        "%{LibraryDir.QT}/include/QtUiTools",
        "%{LibraryDir.QT}/include/QtWidgets",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.ENTT}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.FMOD}",
        "%{IncludeDir.IMGUI}",
        "%{IncludeDir.IMGUIZMO}",
        "%{IncludeDir.IMGUI_NODE}",
        "%{IncludeDir.MONO}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.NVRHI}",
        "%{IncludeDir.STB}",
        "%{IncludeDir.NVRHI_VULKAN_HEADERS}",
        "%{IncludeDir.NVRHI_DIRECTX_HEADERS}",
        "%{IncludeDir.VULKAN_SDK}",
        "%{IncludeDir.FILEWATCHER}",
        "%{IncludeDir.ZLIB}",
        "%{IncludeDir.YAMLCPP}",
        "%{IncludeDir.ASSIMP}",
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.MSDFATLASGEN}",
        "%{IncludeDir.OPENEXR}",
        "%{IncludeDir.IMATH}",
        "%{IncludeDir.MSDFGEN}",
        "%{IncludeDir.FREETYPE}",
        "%{IncludeDir.TRACY}",
        "%{IncludeDir.JSON}",
        "%{IncludeDir.NUKLEAR}",
        "%{IncludeDir.MochiSharpNative}",
        "%{IncludeDir.Hostfxr}"
    }

    defines {
        "VULKAN_HPP_NO_SPACESHIP_OPERATOR",
        "NVRHI_SHARED_LIBRARY_INCLUDE",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
        "JPH_DEBUG_RENDERER",
        "JPH_PROFILE_ENABLED",
        "JPH_OBJECT_STREAM",
    }

    --linux

    --windows
     filter { "system:windows", "toolset:msc*"}
        disablewarnings { "4099" }
        buildoptions {
            "/utf-8",
            "/Zc:__cplusplus"
        }

    filter "system:windows"
    libdirs {
        "%{LibraryDir.QT}/lib"
    }
    defines {
        "PLATFORM_WINDOWS",
        "IGNITE_CUSTOM_ENTRY_POINT",
        "IGNITE_WITH_DX12",
        "IGNITE_WITH_VULKAN",
        "NOMINMAX",
        "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
        "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS",
        "_CRT_SECURE_NO_WARNINGS"
    }
    
    links { "d3dcompiler", "dxcompiler", "delayimp" }
    postbuildcommands {
        'cmake -E make_directory "%{cfg.targetdir}/platforms"',
        '{COPYFILE} "%{LibraryDir.QT}/Qt6Core.dll" "%{cfg.targetdir}"',
        '{COPYFILE} "%{LibraryDir.QT}/Qt6Gui.dll" "%{cfg.targetdir}"',
        '{COPYFILE} "%{LibraryDir.QT}/Qt6UiTools.dll" "%{cfg.targetdir}"',
        '{COPYFILE} "%{LibraryDir.QT}/Qt6Widgets.dll" "%{cfg.targetdir}"',
        '{COPYFILE} "%{wks.location}/resources/qt_schema/MainWindow.ui" "%{cfg.targetdir}/MainWindow.ui"',
        '{COPYFILE} "%{LibraryDir.QT}/plugins/platforms/qwindows.dll" "%{cfg.targetdir}/platforms/qwindows.dll"'
    }

    filter "configurations:Debug"
        runtime "Debug"
        optimize "off"
        symbols "on"
        defines {
            "IGN_DEBUG_BUILD",
            "DEBUG",
            "_DEBUG"
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        symbols "on"
        defines {
            "IGN_RELEASE_BUILD",
            "NDEBUG"
        }

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"
        symbols "off"
        defines {
            "IGN_SHIPPING_BUILD",
            "NDEBUG"
        }
