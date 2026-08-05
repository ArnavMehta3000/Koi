
option("WithEditor")
    set_default(1)
    set_values(0, 1)
    set_showmenu(true)
    set_description("Build with editor support (enables koi-editor target, hot-reload, editor subsystem)")
    set_configvar("WITH_EDITOR", 0)
    after_check(function(option)
        if option:enabled() then
            option:set("configvar", "WITH_EDITOR", option:value())
        end
    end)
option_end()

option("LogLevel")
    set_default(2)
    set_values(0, 1, 2, 3, 4)
    set_description([[Compile time switch to set log level (levels below the floor compile out):
        0 - trace (very verbose: per-resource, per-primitive, per-entity)
        1 - debug (verbose: per-operation detail)
        2 - info/default (lifecycle milestones)
        3 - warn
        4 - error]])

    set_configvar("LOG_LEVEL", -1)
    after_check(function(option)
       if option:enabled() then
            option:set("configvar", "LOG_LEVEL", option:value())
        end
    end)
option_end()
