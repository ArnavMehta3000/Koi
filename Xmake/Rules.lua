includes("ShaderRule.lua")

-- A Koi game is always its own executable. With the editor it links Koi.Editor and bakes in
-- its source dirs. Standalone it runs from cooked content beside the exe (or the rundir)
rule("KoiGame")
	on_load(function(target)
		local _we         = get_config("WithEditor")
		local with_editor = (_we == 1 or _we == "1" or _we == true)
		local root        = target:scriptdir()
		local launcher    = path.join(os.projectdir(), "Launcher")

		target:set("kind", "binary")
		target:set("installdir", path.join(root, "Packaged"))
		target:add("deps", "Koi.Engine", "ImGui")
		target:add("includedirs", path.join(root, "Source"))
		target:add("extrafiles",
			path.join(root, target:name() .. ".koi.toml"),
			path.join(root, "Content/**.toml"),
			path.join(root, "Content/**.hlsl"),
			path.join(root, "Content/**.gltf"))

		local shaders = path.join(root, "Content", "Shaders", "Shaders.json")
		if os.isfile(shaders) then
			target:add("files", shaders, { rule = "KoiShaders", koi_scheme = "project" })
		end

		if with_editor then
			target:set("basename", target:name() .. "-Editor")
			target:add("deps", "Koi.Editor")
			target:add("files", path.join(launcher, "EditorMain.cpp"))
			target:add("defines",
				'KOI_PROJECT_DIR="'  .. path.unix(root) .. '"',
				'KOI_PROJECT_FILE="' .. path.unix(path.join(root, target:name() .. ".koi.toml")) .. '"')
		else
			target:add("files", path.join(launcher, "RuntimeMain.cpp"))
			target:set("rundir", path.join(root, ".koi", "Cooked"))
			if not is_mode("debug", "releasedbg") then
				target:add("rules", "win.sdk.application")
			end
		end
	end)

	-- The editor mounts each target's compiled shaders beside its source tree. autogendir() is relative to
	-- the repo, and the editor runs from elsewhere, hence absolute
	on_config(function(target)
		local _we = get_config("WithEditor")
		if not (_we == 1 or _we == "1" or _we == true) then return end

		local engine = assert(target:dep("Koi.Engine"), "%s doesn't depend on Koi.Engine", target:name())
		target:add("defines",
			'KOI_ENGINE_BUILD_DIR="'  .. path.unix(path.absolute(path.join(engine:autogendir(), "Built"), os.projectdir())) .. '"',
			'KOI_PROJECT_BUILD_DIR="' .. path.unix(path.absolute(path.join(target:autogendir(), "Built"), os.projectdir())) .. '"')
	end)

	-- `xmake install <Game>` assembles a self-contained folder: exe + cooked Engine/ + Project/
	on_install(function(target)
		local _we = get_config("WithEditor")
		if _we == 1 or _we == "1" or _we == true then
			cprint("${yellow}%s: Ensure assets are cooked with editor, then configure with --WithEditor=0 to package the game. Failed to install (bad config: --WithEditor=1)", target:name())
			return
		end

		local cooked = path.join(target:scriptdir(), ".koi", "Cooked")
		assert(os.isfile(path.join(cooked, "Project", "Project.koi.toml")),
			"%s: no cooked content at %s (Cook from the editor first)", target:name(), cooked)

		local dst = target:installdir()
		os.tryrm(path.join(dst, "Engine"))
		os.tryrm(path.join(dst, "Project"))
		os.mkdir(dst)
		os.cp(target:targetfile(), dst)
		if os.isfile(target:symbolfile()) then
			os.cp(target:symbolfile(), dst)
		end
		os.cp(path.join(cooked, "**"), dst, { rootdir = cooked })
		cprint("${green}%s: packaged to %s", target:name(), dst)
	end)

	on_uninstall(function(target)
		local dst = target:installdir()
		os.tryrm(dst)  -- Remove the entire install directory
		--os.tryrm(path.join(dst, "Engine"))
		--os.tryrm(path.join(dst, "Project"))
		--os.tryrm(path.join(dst, path.filename(target:targetfile())))
		--os.tryrm(path.join(dst, path.filename(target:symbolfile())))
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
