target("Sandbox")
    set_kind("static")
    add_files("SandboxApp.cpp")
    add_deps("Koi.Engine")

    add_extrafiles(
    	"Shaders/**.hlsl",
     	"ShaderBuild.json"
    )

    after_build(function(target)
        import("core.base.task")
        task.run("compile_shaders",
        {
            src_path    = path.join(os.scriptdir(), "Shaders"),
            cfg_file    = path.join(os.scriptdir(), "ShaderBuild.json"),
            out_dir     = path.join(target:targetdir(), "Assets"),
            target_name = target:name(),
        })
    end)
target_end()
