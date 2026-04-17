target("Koi.Engine")
    set_kind("static")

    add_options("LogLevel")
    add_packages("Kirin", "spdlog", "glfw", { public = true })

    add_includedirs(".", { public = true })
    add_headerfiles("**.h")
    add_files("**.cpp")

    local base_config_dir = "$(builddir)/Generated/$(mode)-$(arch)/Engine/"

    set_configdir(base_config_dir)
    add_includedirs(base_config_dir, { public = true })
    add_headerfiles(base_config_dir .. "Core/Config/**.h")
    add_configfiles("$(projectdir)/Xmake/ConfigFiles/**.h.in", { prefixdir = "Core/Config" })

    if is_plat("windows") then
        add_syslinks("user32", "dwmapi", { public = true })
    end
target_end()
