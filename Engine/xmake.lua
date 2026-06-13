target("Koi.Engine")
	set_kind("static")

	add_options("LogLevel", "WithEditor")
	add_deps("ImGui", { public = true })
	add_packages(
		"cli11",
		"spdlog",
		"toml++",
		"dxc",
		"stb",
		"hlslppmath",
		"tracy-profiler",
		"glfw",
		"fastgltf",
		"tracy",
		"directx-headers", { public = true })

	add_includedirs(".", { public = true })
	add_headerfiles("**.h", { public = true })
	add_files("**.cpp", { public = true })

	local base_config_dir = "$(builddir)/Generated/$(mode)-$(arch)"
	local config_dir = path.join(base_config_dir, "Generated/Engine")

	set_configdir(config_dir)
	add_includedirs(base_config_dir, { public = true })
	add_headerfiles(base_config_dir, { public = true })
	add_configfiles("$(projectdir)/Xmake/ConfigFiles/**.h.in")

	if is_plat("windows") then
		add_syslinks(
		"user32",
		"dwmapi",
		"d3d12",
		"dxgi",
		"dxguid",
		"d3dcompiler", { public = true })
	end

	add_extrafiles("Assets/Shaders/**.hlsl")
target_end()
