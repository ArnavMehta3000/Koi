import("core.base.option")
import("core.project.config")
import("core.project.project")

local template_dir = path.join(os.scriptdir(), "Template")

-- Every <dir>/<Name>/<Name>.koi.toml under Samples/ and Projects/
local function discover()
	local found = {}
	for _, group in ipairs({ "Samples", "Projects" }) do
		for _, marker in ipairs(os.files(path.join(os.projectdir(), group, "*", "*.koi.toml"))) do
			local name = path.basename(marker):gsub("%.koi$", "")
			if name == path.filename(path.directory(marker)) then
				table.insert(found, { name = name, group = group, dir = path.directory(marker) })
			end
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
		cprint("  ${bright}%-20s${clear} %s/  ${dim}%s", p.name, p.group, p.dir)
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
	print("  xmake f --WithEditor=0 && xmake run " .. name .. "   standalone (Cook from the editor first)")
	print("  xmake install " .. name .. "                      package to Projects/" .. name .. "/Packaged")
end

function main()
	config.load()
	local command = option.get("command") or "list"
	if command == "new" then
		cmd_new(option.get("name"))
	elseif command == "list" then
		cmd_list()
	else
		raise("unknown command '%s' (expected new or list)", command)
	end
end
