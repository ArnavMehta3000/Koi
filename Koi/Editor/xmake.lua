local _we = get_config("WithEditor")
if _we == 1 or _we == "1" or _we == true then

target("Koi.Editor")
	set_kind("static")
	add_deps("Koi.Engine", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_headerfiles("**.h", { public = true })
	add_files("**.cpp")
target_end()

end
