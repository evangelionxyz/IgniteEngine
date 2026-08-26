project "Project Generator"
	location "%{wks.location}/scripts"
	kind "Utility"

	files {
		-- Project Generator & Thirdparty Project
		"%{prj.location}/**.lua",
		"%{prj.location}/**.py",

		-- Core Project
		"%{wks.location}/ignite/**.lua",
		"%{wks.location}/scriptengine/ignite.scriptengine.lua",
		
		-- Project Root
		"%{wks.location}/dockerfile",
		"%{wks.location}/README.md",
		"%{wks.location}/LICENSE.txt",
		"%{wks.location}/gen.bat",
		"%{wks.location}/gen.sh",

		-- CI File
		"%{wks.location}/.github/workflows/ci.yml",
	}

	filter "system:windows"
		prebuildcommands {
			-- "premake5 vs2026 --file=%{wks.location}/scripts/premake5.lua",
			-- "premake5 vs2026 --file=%{wks.location}/scripts/premake5-managed.lua",
		}

	filter "system:linux"
		prebuildcommands {
			-- "premake5 gmake --file=%{wks.location}/scripts/premake5.lua",
			-- "premake5 gmake --file=%{wks.location}/scripts/premake5-managed.lua",
		}
