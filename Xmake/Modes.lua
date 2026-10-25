-- <config>x<editor> | Every mode keeps a PDB
local kModes = {
	{ name = "editor-debug",      optimize = "none" },
	{ name = "editor-releasedbg", optimize = "fastest", ndebug = true },
	{ name = "editor-profile",    optimize = "fastest", ndebug = true, profile = true },
	{ name = "game-debug",        optimize = "none" },
	{ name = "game-release",      optimize = "fastest", ndebug = true, strip = true },
	{ name = "game-profile",      optimize = "fastest", ndebug = true, profile = true },
}

local names = {}
for _, mode in ipairs(kModes) do
	table.insert(names, mode.name)
end
set_allowedmodes(names)
set_defaultmode("editor-debug")

rule("KoiMode")
	on_config(function(target)
		for _, mode in ipairs(kModes) do
			if is_mode(mode.name) then
				target:set("symbols", "debug")
				target:set("optimize", mode.optimize)
				if mode.strip then
					target:set("strip", "all")
				end
				if mode.ndebug then
					target:add("defines", "NDEBUG")
				end
				if mode.profile then
					target:add("ldflags", "/profile", { force = true })
				end
			end
		end
	end)
rule_end()
