package("Kirin")
    set_sourcedir(path.join(os.projectdir(), "External", "kirin"))
    set_description("The kirin package")

    add_configs("shared",     { description = "Build shared library.", default = false, type = "boolean", readonly = true })
    add_configs("exceptions", { description = "Enable exceptions",     default = true,  type = "boolean" })

    on_load(function (package)
        --package:set("installdir", path.join(os.scriptdir(), package:plat(), package:arch(), package:mode()))
    end)

    on_install(function (package)
        local configs = {}
        configs.shared     = package:config("shared")
        configs.exceptions = package:config("exceptions")
        import("package.tools.xmake").install(package, configs)
    end)
package_end()
