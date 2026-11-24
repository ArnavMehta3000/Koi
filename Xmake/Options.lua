option("LogLevel")
    set_default(0)
    set_values(0, 1, 2, 3, 4)
    set_description([[Compile time switch to set log level (levels below the floor compile out):
        0 - trace (default, keeps everything: individual resources, shaders, entities)
        1 - debug (what the engine did, once per operation)
        2 - info (outcomes whoever is making the game cares about)
        3 - warn
        4 - error]])

    after_check(function(option)
        option:set("configvar", "LOG_LEVEL", option:value())
    end)
option_end()
