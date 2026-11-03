option("LogLevel")
    set_default(0)
    set_values(0, 1, 2, 3, 4)
    set_description([[Compile time switch to set log level (levels below the floor compile out):
        0 - trace (very verbose: per-resource, per-primitive, per-entity)
        1 - debug (verbose: per-operation detail)
        2 - info/default (lifecycle milestones)
        3 - warn
        4 - error]])

    after_check(function(option)
        option:set("configvar", "LOG_LEVEL", option:value())
    end)
option_end()
