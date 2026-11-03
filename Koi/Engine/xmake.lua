target("Koi.Engine")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("SimpleMath", { public = true })
	add_deps("GameInput")

	add_packages(
		"cli11",
		"stduuid",
		"spdlog",
		"toml++",
		"dxc",
		"tracy-profiler",
		"Flecs",
		"box3d",
		"reflect-cpp",
		"directx-headers", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_headerfiles("**.h", { public = true })
	add_files("**.cpp", { public = true })
	set_pcxxheader("Pch.h")

	local base_config_dir = "$(builddir)/Generated/$(mode)-$(arch)"
	local config_dir = path.join(base_config_dir, "Generated/Engine")

	set_configdir(config_dir)
	add_includedirs(base_config_dir, { public = true })
	add_headerfiles(base_config_dir, { public = true })

	set_configvar("ENGINE_DIR", path.unix(path.join(os.projectdir(), "Koi", "Engine")))
	set_configvar("BUILD_MODE", get_config("mode"))
	set_configvar("DEBUG", is_mode(".*-debug") and 1 or 0)
	set_configvar("PROFILING", is_mode(".*-profile") and 1 or 0)
	add_configfiles("$(projectdir)/Xmake/ConfigFiles/Config.h.in")

	if is_plat("windows") then
		add_syslinks(
			"user32",
			"shell32",
			"dwmapi",
			"d3d12",
			"dxgi",
			"dxguid",
			"d3dcompiler",
			"dbghelp", { public = true })
	end

	add_extrafiles("Content/Shaders/**.hlsl")
	add_files("Content/Shaders/Shaders.json", { rule = "KoiShaders", koi_scheme = "engine" })
target_end()
