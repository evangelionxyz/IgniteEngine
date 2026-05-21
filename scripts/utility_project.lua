project "Project Generator"
	location "%{wks.location}/scripts"
	kind "Utility"

	files {
		"%{prj.location}/**.lua",
		"%{prj.location}/**.py",

		"%{prj.location}/editor/ignite-editor.lua",
		"%{prj.location}/engine/ignite-engine.lua",
		"%{prj.location}/scriptengine/ignite-scriptengine.lua",
	}

	filter "system:windows"
		postbuildcommands { "premake5 vs2026 --file=%{wks.location}/scripts/premake5.lua" }

	filter "system:linux"
		postbuildcommands { "premake5 gmake --file=%{wks.location}/scripts/premake5.lua" }
