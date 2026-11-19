-- From Koi subdir
local kCoreDirs     = { "Core", "Diagnostics", "IO", "Logging", "Math", "Memory", "Text" }
local kPlatformDirs = { "Platform" }
local kEngineDirs   = { "Assets", "ECS", "Graphics", "Physics", "Runtime" }

-- Koi.RHI's part of Graphics/: whole folders end in /**, the rest are file stems
local kRHIGraphics = { "RHI/**", "RenderGraph/**", "Format", "GpuFrameBuffer", "PipelineCache", "ShaderCache", "ShaderCompiler", "Types" }

-- Koi.Assets' part of Assets/: the CPU file formats. The GPU caches and AssetRegistry stay in Koi.Engine
local kCPUAssets = { "AssetCache", "AssetData", "AssetFile", "AssetHandles", "AssetIndex", "AssetKey", "AssetTraits", "AssetTypes",
	"MaterialFile", "MaterialSource", "MeshFile", "TextureFile" }

-- Koi.ECS' part of ECS/: the storage core. World, System, Observer and the registries stay in Koi.Engine
local kECSCore = { "ECS" }

-- `exclude` holds stems relative to each dir, as in kRHIGraphics
local function add_dirs(dirs, exclude)
	for _, dir in ipairs(dirs) do
		local function pattern(ext)
			local result = dir .. "/**" .. ext
			for _, stem in ipairs(exclude and exclude[dir] or {}) do
				result = result .. "|" .. stem .. ext
			end
			return result
		end
		add_headerfiles(pattern(".h"), pattern(".inl"), { public = true })
		add_files(pattern(".cpp"))
	end
end

local function add_stems(dir, stems)
	for _, stem in ipairs(stems) do
		for _, ext in ipairs({ ".h", ".inl" }) do
			add_headerfiles(dir .. "/" .. stem .. ext, { public = true })
		end
		local cpp = dir .. "/" .. stem .. ".cpp"
		if stem:endswith("**") or os.isfile(path.join(os.scriptdir(), cpp)) then
			add_files(cpp)
		end
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
	add_stems("Graphics", kRHIGraphics)
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
	add_stems("Assets", kCPUAssets)
	set_pcxxheader("CorePch.h")
target_end()

-- ECS
target("Koi.ECS")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("Koi.Core", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_stems("ECS", kECSCore)
	set_pcxxheader("CorePch.h")
target_end()

--ENGINE
target("Koi.Engine")
	set_kind("static")
	set_group("Koi")
	add_options("LogLevel")

	add_deps("Koi.Core", "Koi.Platform", "Koi.RHI", "Koi.Assets", "Koi.ECS", { public = true })
	add_packages("cli11", "box3d", { public = true })

	add_includedirs("$(projectdir)", { public = true })
	add_dirs(kEngineDirs, { Graphics = kRHIGraphics, Assets = kCPUAssets, ECS = kECSCore })
	set_pcxxheader("Pch.h")

	add_extrafiles("Content/Shaders/**.hlsl")
	add_files("Content/Shaders/Shaders.json", { rule = "KoiShaders", koi_scheme = "engine" })
target_end()
