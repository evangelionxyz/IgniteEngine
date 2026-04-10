-- Copyright (c) 2026 Evangelion Manuhutu

project "freetype"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C"
    staticruntime "off"

    defines {
        "DLL_EXPORT",
        "FT2_BUILD_LIBRARY"
    }

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/include/ft2build.h",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/include/freetype/**.h",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/include/freetype/config/**.h",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/include/freetype/internal/**.h",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/autofit/autofit.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftbase.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftbbox.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftbdf.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftbitmap.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftcid.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftdebug.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftfstype.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftgasp.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftglyph.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftgxval.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftinit.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftmm.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftotval.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftpatent.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftpfr.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftstroke.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftsynth.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftsystem.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/fttype1.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/base/ftwinfnt.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/bdf/bdf.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/bzip2/ftbzip2.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/cache/ftcache.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/cff/cff.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/cid/type1cid.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/gzip/ftgzip.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/lzw/ftlzw.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/pcf/pcf.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/pfr/pfr.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/psaux/psaux.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/pshinter/pshinter.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/psnames/psnames.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/raster/raster.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/sdf/sdf.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/svg/svg.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/sfnt/sfnt.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/smooth/smooth.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/truetype/truetype.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/type1/type1.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/type42/type42.c",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/src/winfonts/winfnt.c"
    }

    includedirs { "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/include"}
    
    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"