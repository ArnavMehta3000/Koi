-- User rule to switch between static/object build kind for runtime/editor modes + sets up runargs
rule("KoiGame")
	on_load(function(target)
		local _we = get_config("WithEditor")
		local with_editor = (_we == 1 or _we == "1" or _we == true)

		if with_editor then
			target:set("kind", "object")
		else
			target:set("kind", "static")
		end

		target:add("deps", "Koi.Engine", "ImGui")
	end)

	on_config(function(target)
		import("core.project.project")

		local _we           = get_config("WithEditor")
		local with_editor   = (_we == 1 or _we == "1" or _we == true)
		local launcher_name = with_editor and "koi-editor" or "koi-runtime"  -- Toggle between editor or runtime project
		local launcher      = project.target(launcher_name)

		if not launcher then return end

		-- Add self as dependency + get project TOML filepath
		launcher:add("deps", target:name())
		local project_file = path.join(target:scriptdir(), target:name() .. ".koi.toml")

		if with_editor then
			-- Source-tree roots let the editor read .hlsl / .gltf / scene
			-- TOMLs directly from disk without any copy step.
			local engine_target = project.target("Koi.Engine")
			local engine_src    = engine_target and engine_target:scriptdir() or ""

			launcher:set("runargs",
				"-p", project_file,
				"--engine-source",  engine_src,
				"--project-source", target:scriptdir())
		else
			launcher:set("runargs", "-p", project_file)
		end
	end)
rule_end()

-- Generate RDI file using raddbg/radbin
rule("GenerateRDI")
	after_build(function(target)
		if target:kind() == "binary"  and  (target:get("mode") ~= "release")then

			import("lib.detect.find_program")

			local program = find_program("radbin")
			if program then
				local cmd = program .. " --rdi " .. target:targetfile()
				os.exec(cmd)
			end
		end
	end)
rule_end()

-- Add natvis files from packages
rule("Natvis")
	on_config(function (target)
		for name, pkg in pairs(target:pkgs()) do
			local includedir = path.join(pkg:installdir(), "include")
			local natvis = os.files(path.join(includedir, "**.natvis"))
			if #natvis > 0 then
				local groups = table.wrap(target:get("filegroups"))
				if not table.find(groups, name) then
					target:add("extrafiles", natvis, { install = false })
					target:add("filegroups", path.join("nativs", name), { rootdir = includedir })
				end
			end
		end
	end)
rule_end()
