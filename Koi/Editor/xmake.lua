target("Koi.Editor")
	set_group("Koi")
	set_kind("static")
	set_default(is_mode("editor-.*"))
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
