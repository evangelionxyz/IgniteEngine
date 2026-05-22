project "Project Generator"
	location "%{wks.location}/scripts"
	kind "Utility"

	files {
		-- Project Generator & Thirdparty Project
		"%{prj.location}/**.lua",
		"%{prj.location}/**.py",

		-- Core Project
		"%{wks.location}/editor/ignite-editor.lua",
		"%{wks.location}/engine/ignite-engine.lua",
		"%{wks.location}/scriptengine/ignite-scriptengine.lua",
		
		-- Dockerfile
		"%{wks.location}/dockerfile",

		-- CI File
		"%{wks.location}/.github/workflows/ci.yml",
	}

	filter "system:windows"
		prebuildcommands { "premake5 vs2026 --file=%{wks.location}/scripts/premake5.lua" }

	filter "system:linux"
		prebuildcommands { "premake5 gmake --file=%{wks.location}/scripts/premake5.lua" }
