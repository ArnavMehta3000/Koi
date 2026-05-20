set_xmakever("3.0.8")
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.profile")
add_rules("plugin.compile_commands.autoupdate")

set_project("Koi")
set_version("0.0.1", { build = "%Y%m%d%H%M" })

add_plugindirs("./Plugins")

set_languages("c17", "cxx23")
set_defaultmode("debug")
set_warnings("all", "extra")
set_policy("build.warning", true)
set_policy("run.autobuild", true)

includes("Xmake/Packages.lua")
includes("Xmake/Options.lua")
includes("Xmake/Rules.lua")

add_cxxflags("/utf-8", { public = true })

add_defines("KOI_ENABLE_PROFILING=" .. (is_mode("profile") and "1" or "0"))

-- Windows defines
add_defines("NOMINMAX", "UNICODE", "_UNICODE", "NOMCX", "NOSERVICE", "NOHELP", "WIN32_LEAN_AND_MEAN")
add_rules("GenerateRDI", "Natvis")

includes("Engine")
includes("Launcher")
includes("Sandbox")
