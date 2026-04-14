set_xmakever("3.0.8")
add_rules("mode.debug", "mode.release",
		"mode.releasedbg", "mode.profile")

set_project("Koi")
set_version("0.0.1", { build = "%Y%m%d%H%M" })

add_plugindirs(path.join(os.projectdir(), "Plugins"))

set_languages("c17", "cxx23")
set_defaultmode("debug")

set_warnings("all", "extra")
set_policy("build.warning", true)
set_policy("run.autobuild", true)

includes("Xmake/Packages.lua")
--includes("Xmake/Options.lua")

target("Koi")
    add_packages("Kirin", "spdlog")
    set_kind("binary")
    add_includedirs("Source/Koi")
    add_headerfiles("Source/Koi/**.h")
    add_files("Source/Koi/**.cpp")

    set_configdir("$(builddir)/Generated")
    add_includedirs("$(builddir)/Generated")
    add_headerfiles("$(builddir)/Generated/**.h")

    add_configfiles("Xmake/ConfigFiles/Version.h.in", { prefixdir = "Config" })

    if is_plat("windows") then
        add_syslinks("user32", "dwmapi")
    end

target_end()
