project "Project Generator"
	location "%{wks.location}/scripts"
	kind "Utility"

	files {
		"%{prj.location}/**.lua",
		"%{prj.location}/**.py",
	}

	postbuildcommands {
	
	}

