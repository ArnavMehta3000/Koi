-- Uses a 'Shaders.json' to build shaders to <autogendir>/Built/Shaders along with 'Manifest.toml'
--
-- A complete Shaders.json. Only "source", "defines" and "permutations" are read today; [planned] keys are not
-- {
--   "defines":  ["KOI_BINDLESS=1"],                  [planned] added to every shader in this file
--   "includes": ["Common"],                          [planned] extra -I dirs, relative to this file
--   "shaders": [
--     {
--       "source":  "GBufferPS.hlsl",                 the stage comes from the name's last two letters (VS/PS/CS/HS/DS/GS)
--       "entry":   "PSMain",                         [planned] overrides the <Stage>Main default
--       "model":   "6_6",                            [planned] shader model, so the profile is ps_6_6
--       "defines": ["USE_PARALLAX=0", "MAX_LIGHTS=16"],
--       "permutations": {                            one variant per combination: 2 x 3 = 6 here
--         "ALPHA_TEST":  [false, true],              booleans become 0/1
--         "SHADOW_TAPS": [1, 4, 16]
--       },
--       "exclude": [ { "ALPHA_TEST": false, "SHADOW_TAPS": 16 } ],  [planned] combinations not to build
--       "args":    ["-enable-16bit-types"]           [planned] passed straight to dxc
--     }
--   ]
-- }
rule("KoiShaders")
	set_extensions(".json")

	on_build_file(function (target, sourcefile, opt)  -- Maybe move this to pre/post build step instead?
		import("core.base.json")
		import("core.base.option")
		import("core.project.depend")
		import("async.runjobs")
		import("utils.progress")

		local scheme = target:fileconfig(sourcefile).koi_scheme
		assert(scheme, "%s: add Shaders.json with { rule = \"KoiShaders\", koi_scheme = \"engine\" or \"project\" }", target:name())

		sourcefile           = path.absolute(sourcefile)
		local shaderdir      = path.directory(sourcefile)                 -- <root>/Content/Shaders
		local root           = path.directory(path.directory(shaderdir))  -- what VFS <scheme>:// points at
		local enginedir      = path.join(os.projectdir(), "Koi", "Engine")
		local outdir         = path.join(target:autogendir(), "Built", "Shaders")
		local isdebug        = is_mode("debug")

		local function find_dxc()
			for _, owner in ipairs({ target, target:dep("Koi.Engine") }) do
				local pkg = owner and owner:pkg("dxc")
				if pkg then return path.join(pkg:installdir(), "bin", "dxc.exe") end
			end
			import("lib.detect.find_tool")
			local tool = find_tool("dxc")
			return tool and tool.program
		end
		local dxc = assert(find_dxc(), "Failed to find the DXC compiler!")

		-- Must match ShaderCompiler.cpp, which compiles the same sources at runtime for F5 (hot-reload)
		local stages = { VS = { "vs_6_6", "VSMain" }, PS = { "ps_6_6", "PSMain" }, CS = { "cs_6_6", "CSMain" },
		                 HS = { "hs_6_6", "HSMain" }, DS = { "ds_6_6", "DSMain" }, GS = { "gs_6_6", "GSMain" } }

		local includedirs = { root }
		if root ~= enginedir then table.insert(includedirs, enginedir) end

		-- Add common defines
		local common = { "-WX", "-all_resources_bound", "-D", isdebug and "KOI_DEBUG=1" or "KOI_DEBUG=0" }
		for _, dir in ipairs(includedirs) do table.join2(common, { "-I", dir }) end  -- Add include dirs

		local variants = {}

		-- For each shader entry
		for _, entry in ipairs(json.loadfile(sourcefile).shaders or {}) do
			assert(entry.source, "%s has a shader without a \"source\"", sourcefile)
			local src   = path.join(shaderdir, entry.source)
			local stem  = path.basename(src)
			local stage = stages[stem:sub(-2)]
			assert(stage, "%s: can't tell what stage %s is. End its name in VS, PS, CS, HS, DS or GS", sourcefile, entry.source)  -- Maybe eventually support mesh shaders?
			assert(os.isfile(src), "%s lists %s, but there's no such file", sourcefile, entry.source)

			local defines = {}
			for _, define in ipairs(entry.defines or {}) do table.join2(defines, { "-D", define }) end

			-- Every combination of the permutation values, keys sorted so the order never shuffles
			local permutations = entry.permutations or {}

			-- JSON numbers arrive as floats, and HLSL wants 1 rather than 1.0
			local function define_value(value)
				if type(value) == "boolean" then return value and "1" or "0" end
				if type(value) == "number" and value == math.floor(value) then return string.format("%d", value) end
				return tostring(value)
			end

			local combos = { {} }
			for _, name in ipairs(table.orderkeys(permutations)) do
				local grown = {}
				for _, combo in ipairs(combos) do
					for _, value in ipairs(permutations[name]) do
						local next = table.copy(combo)
						table.insert(next, name .. "=" .. define_value(value))
						table.insert(grown, next)
					end
				end
				combos = grown
			end

			local reldir  = path.directory(path.relative(src, shaderdir))
			local virtual = scheme .. "://" .. (path.unix(path.relative(src, root)):gsub("%.hlsl$", ""))

			for _, combo in ipairs(combos) do
				local suffix = #combo > 0 and ("_" .. (table.concat(combo, "_"):gsub("[^%w]", "_"))) or ""
				local base   = (reldir == "." or reldir == "") and (stem .. suffix) or path.join(reldir, stem .. suffix)
				local cso    = path.join(outdir, base .. ".cso")

				local flags = table.join({ "-T", stage[1], "-E", stage[2] }, common, defines)
				for _, define in ipairs(combo) do table.join2(flags, { "-D", define }) end

				local args = table.join(flags, isdebug and { "-Zi", "-Fd", path.join(outdir, base .. ".pdb") } or { "-O3" },
					{ "-Qstrip_reflect", "-Fo", cso, "-Fre", path.join(outdir, base .. ".refl"), src })

				local dependfile = target:dependfile(cso)
				local dependinfo = option.get("rebuild") and {} or (depend.load(dependfile) or {})
				local values     = table.join({ dxc }, args)

				table.insert(variants,
				{
					source     = virtual,
					display    = path.relative(src, root) .. (#combo > 0 and (" [" .. table.concat(combo, " ") .. "]") or ""),
					sourcefile = src,
					combo      = combo,
					cso        = cso,
					refl       = path.join(outdir, base .. ".refl"),
					flags      = flags,
					args       = args,
					values     = values,
					dependfile = dependfile,
					dependinfo = dependinfo,
					changed    = not os.isfile(cso) or depend.is_changed(dependinfo, { lastmtime = os.mtime(cso), values = values }),
				})
			end
		end

		-- dxc -MF only lists includes and skips the compile, so it's a second, cheap call
		local function find_includes(variant)
			local depfile = os.tmpfile() .. ".d"
			os.execv(dxc, table.join(variant.flags, { "-MF", depfile, variant.sourcefile }), { try = true, stdout = os.nuldev(), stderr = os.nuldev() })

			local files = { variant.sourcefile }
			local text  = os.isfile(depfile) and io.readfile(depfile) or ""
			os.tryrm(depfile)

			local colon = text:find(": ", 1, true)  -- not the drive letter's colon
			if not colon then return files end

			for dep in text:sub(colon + 2):gsub("\\\r?\n", " "):gmatch("%S+") do
				for _, dir in ipairs(table.join({ path.directory(variant.sourcefile) }, includedirs)) do
					local file = path.is_absolute(dep) and dep or path.join(dir, dep)
					if os.isfile(file) then
						file = path.normalize(file)
						if not table.contains(files, file) then table.insert(files, file) end
						break
					end
				end
			end
			return files
		end

		local failures = {}
		local function build(variant)
			progress.show(opt.progress, "${color.build.object}compiling.shader.$(mode) %s", variant.display)
			os.mkdir(path.directory(variant.cso))

			local log = os.tmpfile()
			local ok  = os.execv(dxc, variant.args, { try = true, stdout = log, stderr = log })
			local output = os.isfile(log) and io.readfile(log) or ""
			os.tryrm(log)

			if ok ~= 0 then
				os.tryrm(variant.cso)  -- so the next build tries again even if nothing changes
				table.insert(failures, { name = variant.display, output = output })
				return
			end

			if output:trim() ~= "" then
				cprint("${color.warning}%s had something to say:${clear}\n%s", variant.display, output)
			end

			variant.dependinfo.files  = find_includes(variant)
			variant.dependinfo.values = variant.values
			depend.save(variant.dependinfo, variant.dependfile)
		end

		local jobs = {}
		for _, variant in ipairs(variants) do
			if variant.changed then table.insert(jobs, variant) end
		end

		if #jobs > 0 then
			runjobs("koi_shaders", function (index) build(jobs[index]) end, { total = #jobs, comax = option.get("jobs") or os.default_njob() })
		end

		if #failures > 0 then
			for _, failure in ipairs(failures) do
				cprint("${color.error}%s didn't compile:${clear}\n%s", failure.name, failure.output)
			end
			raise("%d shader variant(s) failed to compile", #failures)
		end

		-- Dependencies go in as virtual paths so the editor can spot a .cso older than its sources
		local function to_virtual(file)
			for _, mount in ipairs({ { scheme, root }, { "engine", enginedir } }) do
				local rel = path.relative(file, mount[2])
				if not rel:startswith("..") and not path.is_absolute(rel) then
					return mount[1] .. "://" .. path.unix(rel)
				end
			end
			return path.unix(file)
		end

		local function quote(text)
			return '"' .. (text:gsub("\\", "\\\\"):gsub('"', '\\"')) .. '"'
		end

		local function quote_all(items, transform)
			local quoted = {}
			for _, item in ipairs(items) do table.insert(quoted, quote(transform and transform(item) or item)) end
			return "[" .. table.concat(quoted, ", ") .. "]"
		end

		-- Generate manifest TOML
		local lines = { "# Written by the KoiShaders build rule from " .. path.filename(sourcefile) .. ", so edits here get overwritten", "" }
		for _, variant in ipairs(variants) do
			table.join2(lines,
			{
				"[[shader]]",
				"source       = " .. quote(variant.source),
				"defines      = " .. quote_all(variant.combo),
				"cso          = " .. quote(path.unix(path.relative(variant.cso, outdir))),
				"reflection   = " .. quote(path.unix(path.relative(variant.refl, outdir))),
				"dependencies = " .. quote_all(variant.dependinfo.files or { variant.sourcefile }, to_virtual),
				"",
			})
		end

		-- Only touched when something changed
		local manifest = path.join(outdir, "Manifest.toml")
		local text     = table.concat(lines, "\n")
		if not os.isfile(manifest) or io.readfile(manifest) ~= text then
			os.mkdir(outdir)
			io.writefile(manifest, text)
		end

		-- Variants dropped from Shaders.json would otherwise linger and get cooked
		local keep = {}
		for _, variant in ipairs(variants) do keep[path.normalize(path.join(path.directory(variant.cso), path.basename(variant.cso)))] = true end
		for _, file in ipairs(os.files(path.join(outdir, "**")) or {}) do
			local ext = path.extension(file)
			if (ext == ".cso" or ext == ".refl" or ext == ".pdb") and not keep[path.normalize(path.join(path.directory(file), path.basename(file)))] then
				os.rm(file)
				vprint("removed stale shader output %s", file)
			end
		end
	end)
rule_end()
