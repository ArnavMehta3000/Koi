set_xmakever("3.0.8")
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.profile")

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

includes("Engine")
includes("Editor")
includes("Launcher")
includes("Sandbox")
