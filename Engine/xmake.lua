target("Koi.Engine")
    set_kind("static")

    add_options("LogLevel")
    add_packages("Kirin", "spdlog", "glfw", { public = true })

    add_includedirs(".", { public = true })
    add_headerfiles("**.h")
    add_files("**.cpp")

    local base_config_dir = "$(builddir)/Generated/$(mode)-$(arch)/Generated/Engine/"

    set_configdir(base_config_dir)
    add_includedirs("$(builddir)/Generated/$(mode)-$(arch)", { public = true })
    add_headerfiles("$(builddir)/Generated/$(mode)-$(arch)")
    add_configfiles("$(projectdir)/Xmake/ConfigFiles/**.h.in")

    if is_plat("windows") then
        add_syslinks("user32", "dwmapi", { public = true })
    end
target_end()
