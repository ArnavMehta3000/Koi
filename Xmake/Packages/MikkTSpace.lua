target("MikkTSpace")  -- Tangent generation, editor-only
	set_kind("static")
	set_group("External")
	set_default(false)
	set_warnings("none")
	add_files("$(projectdir)/External/MikkTSpace/mikktspace.c")
	add_headerfiles("$(projectdir)/External/MikkTSpace/mikktspace.h")
	add_includedirs("$(projectdir)/External/MikkTSpace", { public = true })
target_end()
