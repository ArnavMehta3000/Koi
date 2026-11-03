target("Koi.Tests")
	set_kind("binary")
	set_group("Koi")
	set_default(false)

	add_deps("Koi.Engine")
	add_packages("doctest", "toml++", "stduuid", "spdlog", "reflect-cpp", "tracy-profiler", "Flecs", "box3d")

	add_files("**.cpp")

	for _, file in ipairs(os.files(path.join(os.scriptdir(), "*Tests.cpp"))) do
		local name = path.basename(file):gsub("Tests$", "")
		add_tests(name, { runargs = { "--source-file=*" .. path.filename(file) } })
	end
target_end()
