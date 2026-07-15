target("ImGui")  -- Custom ImGui package
	set_kind("object")
	set_group("External")

	add_includedirs("$(projectdir)/External/ImGui", { public = true })
	add_includedirs("$(projectdir)/External/ImGui/misc/cpp", { public = true })
	add_files("$(projectdir)/External/ImGui/**.cpp", { public = true })
	add_headerfiles("$(projectdir)/External/ImGui/**.h", { public = true })

	add_packages("glfw")
	add_syslinks("imm32")

	-- Remove misc directory
	remove_files("$(projectdir)/External/ImGui/misc/**.cpp")
	remove_headerfiles("$(projectdir)/External/ImGui/misc/**.h")
target_end()
