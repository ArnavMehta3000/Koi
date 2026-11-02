target("GameInput")  -- github.com/microsoftconnect/GameInput v3.3.195 (dce6dab): v3 header + runtime loader
	set_kind("object")
	set_group("External")

	add_includedirs("$(projectdir)/External/GameInput", { public = true })
	add_headerfiles("$(projectdir)/External/GameInput/GameInput.h")
	add_files("$(projectdir)/External/GameInput/GameInput.cpp")
target_end()
