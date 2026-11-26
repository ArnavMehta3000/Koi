set_xmakever("3.1.1")
add_rules("plugin.compile_commands.autoupdate")
set_policy("generator.compile_commands", true)

set_project("Koi")
set_version("0.0.1", { build = "%Y%m%d%H%M" })

add_plugindirs("./Plugins")

set_languages("c17", "cxx23")
set_toolchains("msvc", { vs = "2026", vs_toolset = "14.52" })
set_warnings("all", "extra")

set_policy("build.warning", true)
set_policy("run.autobuild", true)
set_policy("generator.vsxmake.root_sln", true)
set_policy("package.msbuild.multi_tool_task", true)
set_policy("platform.longpaths", true)
-- set_policy("build.c++.dynamic_debugging", true)  -- Linking fails with this on :(
-- set_policy("build.optimization.lto", true)  -- Dynamic Debugging is incompatible with LTO

includes("Xmake/Modes.lua")
includes("Xmake/Packages.lua")
includes("Xmake/Options.lua")
includes("Xmake/Rules.lua")

add_cxxflags("/utf-8", { public = true })

if is_mode(".*-profile") then
	add_defines("TRACY_ENABLE")
end

-- Windows defines
add_defines("NOMINMAX", "UNICODE", "_UNICODE", "NOMCX", "NOSERVICE", "NOHELP", "WIN32_LEAN_AND_MEAN")
add_rules("KoiMode", "Natvis")

includes("Koi/Engine")
includes("Koi/Editor")
for _, file in ipairs(os.files("Koi/Modules/*/xmake.lua")) do
	includes(file)
end

-- A game project is any Projects/<Name>/ carrying <Name>.koi.toml
for _, marker in ipairs(os.files("Projects/*/*.koi.toml")) do
	local dir = path.directory(marker)
	if path.basename(marker):gsub("%.koi$", "") == path.filename(dir) then
		includes(path.join(dir, "xmake.lua"))
	end
end
