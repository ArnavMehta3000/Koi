local _we = get_config("WithEditor")
if _we == 1 or _we == "1" or _we == true then

target("Koi.Editor")
	set_kind("static")
	add_deps("Koi.Engine", "ImGui", "ImGuizmo", "KoiProfiler", { public = true })

	add_packages("directxtex", "fastgltf", "stb")  -- importers only: the runtime reads Koi's own formats
	add_syslinks("ole32", "d3d11", "dxgi", { public = true })  -- COM for WIC decoding, D3D11 for GPU BC6H/BC7

	add_includedirs("$(projectdir)", { public = true })
	add_headerfiles("**.h", { public = true })
	add_files("**.cpp")

	add_rules("utils.bin2obj")
	add_files("Content/DefaultLayout.ini", { rule = "utils.bin2obj" })
	add_files("Content/Fonts/*.ttf", { rule = "utils.bin2obj" })
target_end()

end
