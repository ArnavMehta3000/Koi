-- From Koi subdir
local kCoreDirs     = { "Core", "Diagnostics", "IO", "Logging", "Math", "Memory", "Text" }
local kPlatformDirs = { "Platform" }
local kEngineDirs   = { "ECS", "Graphics", "Resources", "Runtime" }

-- `skip` names a subfolder of the dir that belongs to another target (ECS/Core is Koi.ECS)
local function add_dirs(dirs, skip)
	for _, dir in ipairs(dirs) do
		local function pattern(ext)
			local result = dir .. "/**" .. ext
			if skip and skip[dir] then
				result = result .. "|" .. skip[dir] .. "/**" .. ext
			end
			return result
		end
		add_headerfiles(pattern(".h"), pattern(".inl"), { public = true })
		add_files(pattern(".cpp"))
	end
end

-- CORE
target("Koi.Core")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("SimpleMath", { public = true })
	add_packages("stduuid", "spdlog", "toml++", "tracy-profiler", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_dirs(kCoreDirs)
	set_pcxxheader("CorePch.h")

	local base_config_dir = "$(builddir)/Generated/$(mode)-$(arch)"
	local config_dir = path.join(base_config_dir, "Generated/Engine")

	set_configdir(config_dir)
	add_includedirs(base_config_dir, { public = true })
	add_headerfiles(base_config_dir, { public = true })

	set_configvar("ENGINE_DIR", path.unix(path.join(os.projectdir(), "Koi", "Engine")))
	set_configvar("BUILD_MODE", get_config("mode"))
	set_configvar("DEBUG", is_mode(".*-debug") and 1 or 0)
	set_configvar("PROFILING", is_mode(".*-profile") and 1 or 0)
	set_configvar("WITH_EDITOR", is_mode("editor-.*") and 1 or 0)
	set_configvar("DEBUG_DRAW", is_mode("game-release") and 0 or 1)
	set_configvar("RELEASE", is_mode("game-release") and 1 or 0)
	add_configfiles("$(projectdir)/Xmake/ConfigFiles/Config.h.in")

	if is_plat("windows") then
		add_syslinks("user32", "dbghelp", { public = true })
	end
target_end()

-- PLATFORM
target("Koi.Platform")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("Koi.Core", { public = true })
	add_deps("GameInput")

	add_includedirs("$(projectdir)", { public = true })
	add_dirs(kPlatformDirs)
	set_pcxxheader("CorePch.h")

	if is_plat("windows") then
		add_syslinks("shell32", "dwmapi", { public = true })
	end
target_end()

-- RHI
target("Koi.RHI")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("Koi.Core", { public = true })
	add_packages("dxc", "directx-headers", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_dirs({ "RHI" })
	set_pcxxheader("Pch.h")

	if is_plat("windows") then
		add_syslinks("d3d12", "dxgi", "dxguid", "d3dcompiler", { public = true })
	end
target_end()

-- ASSETS
target("Koi.Assets")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("Koi.Core", "Koi.RHI", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_dirs({ "Assets" })
	set_pcxxheader("CorePch.h")
target_end()

-- ECS
target("Koi.ECS")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("Koi.Core", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_dirs({ "ECS/Core" })
	set_pcxxheader("CorePch.h")
target_end()

--ENGINE
target("Koi.Engine")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("Koi.Core", "Koi.Platform", "Koi.RHI", "Koi.Assets", "Koi.ECS", { public = true })
	add_deps("ScreenGrab")
	add_packages("cli11", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_dirs(kEngineDirs, { ECS = "Core" })
	set_pcxxheader("Pch.h")

	if is_plat("windows") then
		add_syslinks("ole32", "windowscodecs", { public = true })  -- screenshots are encoded with WIC
	end

	add_extrafiles("Content/Shaders/**.hlsl")
	add_files("Content/Shaders/Shaders.json", { rule = "KoiShaders", koi_scheme = "engine" })
target_end()
