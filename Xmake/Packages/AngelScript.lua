target("AngelScript")
	set_kind("static")
	set_group("External")

	add_includedirs(
		"$(projectdir)/External/AngelScript/include",
    	"$(projectdir)/External/Angelscript/add_on", { public = true })

	add_headerfiles(
		"$(projectdir)/External/AngelScript/include/angelsript.h",
		"$(projectdir)/External/AngelScript/source/**.h", { public = true })

	add_files(
		"$(projectdir)/External/AngelScript/source/**.cpp",
		"$(projectdir)/External/AngelScript/source/**.asm",
		"$(projectdir)/External/Angelscript/add_on/scriptbuilder/scriptbuilder.cpp",
		"$(projectdir)/External/Angelscript/add_on/scriptstdstring/scriptstdstring.cpp",
		"$(projectdir)/External/Angelscript/add_on/weakref/weakref.cpp", { public = true })

target_end()
