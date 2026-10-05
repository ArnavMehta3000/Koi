local _we = get_config("WithEditor")
if _we == 1 or _we == "1" or _we == true then

target("Koi.Editor")
	set_kind("static")
	add_deps("Koi.Engine", "ImGui", "ImGuizmo", "KoiProfiler", { public = true })

	add_packages("directxtex")

	add_includedirs("$(projectdir)", { public = true })
	add_headerfiles("**.h", { public = true })
	add_files("**.cpp")

	add_rules("utils.bin2obj")
	add_files("Content/DefaultLayout.ini", { rule = "utils.bin2obj" })
target_end()

end
