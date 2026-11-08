import("core.base.option")
import("core.project.config")
import("core.project.project")

local template_dir = path.join(os.scriptdir(), "Template")

-- Every Projects/<Name>/<Name>.koi.toml
local function discover()
	local found = {}
	for _, marker in ipairs(os.files(path.join(os.projectdir(), "Projects", "*", "*.koi.toml"))) do
		local name = path.basename(marker):gsub("%.koi$", "")
		if name == path.filename(path.directory(marker)) then
			table.insert(found, { name = name, dir = path.directory(marker) })
		end
	end
	return found
end

local function cmd_list()
	local projects = discover()
	if #projects == 0 then
		print("No projects found")
		return
	end
	for _, p in ipairs(projects) do
		cprint("  ${bright}%-20s${clear} ${dim}%s", p.name, p.dir)
	end
end

local function cmd_new(name)
	assert(name, "usage: xmake koi new <Name>")
	assert(name:match("^[%a_][%w_]*$"), "'%s' is not a valid project name (letters, digits, underscore; cannot start with a digit)", name)
	for _, p in ipairs(discover()) do
		assert(p.name ~= name, "a project named '%s' already exists at %s", name, p.dir)
	end

	local dst = path.join(os.projectdir(), "Projects", name)
	assert(not os.exists(dst), "%s already exists", dst)

	-- Copy the template, substituting {{Name}} in paths and contents
	for _, src in ipairs(os.files(path.join(template_dir, "**"))) do
		local rel = path.relative(src, template_dir):gsub("{{Name}}", name)
		local out = path.join(dst, rel)
		os.mkdir(path.directory(out))
		io.writefile(out, (io.readfile(src):gsub("{{Name}}", name)))
	end

	cprint("${green}Created ${bright}%s${clear}${green} at %s", name, dst)
	print("")
	print("  xmake run " .. name .. "                          editor")
	print("  xmake f -m game-debug && xmake run " .. name .. "     standalone (Cook from the editor first)")
	print("  xmake install " .. name .. "                      package to Projects/" .. name .. "/Packaged")
end

-- Every project + the tests in both shipping configs, ending back on editor-debug
local function cmd_check()
	local xmake   = os.programfile()
	local targets = { "Koi.Tests" }
	for _, p in ipairs(discover()) do
		table.insert(targets, p.name)
	end

	local passed = {}
	local failure = nil
	for _, mode in ipairs({ "editor-debug", "game-release" }) do
		cprint("${bright cyan}== %s: %s", mode, table.concat(targets, " "))
		try
		{
			function ()
				os.execv(xmake, { "f", "-m", mode, "-y" })
				os.execv(xmake, table.join({ "build" }, targets))
				os.execv(xmake, { "test" })
				table.insert(passed, mode)
			end,
			catch { function () failure = mode end }
		}
		if failure then break end
	end

	os.execv(xmake, { "f", "-m", "editor-debug", "-y" })

	for _, mode in ipairs(passed) do
		cprint("${green}ok${clear}     %s", mode)
	end
	if failure then
		cprint("${red}failed${clear} %s (see the output above)", failure)
		os.exit(1)
	end
end

-- The viewer comes from the tracy-tools package, which only profile modes require
local function cmd_tracy()
	local tools = project.required_package("tracy-tools")
	assert(tools and tools:installdir(), "Tracy is only installed in profile modes: run `xmake f -m editor-profile` (or game-profile) first")

	local viewer = path.join(tools:installdir(), "bin", "tracy-profiler.exe")
	assert(os.isfile(viewer), "%s is missing, run `xmake require --force tracy-tools`", viewer)

	cprint("${green}Opening Tracy${clear} ${dim}%s", path.directory(viewer))
	os.execv(viewer, {}, { detach = true })
end

function main()
	config.load()
	local command = option.get("command") or "list"
	if command == "new" then
		cmd_new(option.get("name"))
	elseif command == "list" then
		cmd_list()
	elseif command == "tracy" then
		cmd_tracy()
	elseif command == "check" then
		cmd_check()
	else
		raise("unknown command '%s' (expected new, list, tracy or check)", command)
	end
end
