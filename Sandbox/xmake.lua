target("Sandbox")
	set_kind("static")
	add_deps("Koi.Engine")

	add_includedirs("Scripts")
	add_files("Scripts/**.cpp")
	add_headerfiles("Scripts/**.h")

	-- Shader files
	add_extrafiles(
		"Assets/Shaders/**.hlsl",
		"ShaderBuild.json"
	)

	-- Assets
	add_extrafiles("Assets/Models/**.gltf")
	add_extrafiles("Sandbox.toml")

	after_build(function(target)
		import("core.base.task")
		task.run("compile_shaders",
		{
			src_path    = path.join(os.scriptdir(), "Assets/Shaders"),
			cfg_file    = path.join(os.scriptdir(), "ShaderBuild.json"),
			out_dir     = path.join(target:targetdir(), "Assets"),
			target_name = target:name(),
		})

	-- Copy asset files
	os.cp(
		path.join(os.scriptdir(), "Assets/Models"),
		path.join(target:targetdir(), "Assets/Models"))
	end)
target_end()
