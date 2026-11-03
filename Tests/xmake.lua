-- Engine tests without a window, GPU or Engine instance. `xmake test` builds and runs them
target("Koi.Tests")
	set_kind("binary")
	set_group("Koi")
	set_default(false)

	add_deps("Koi.Engine")
	add_packages("doctest", "toml++", "stduuid", "spdlog", "reflect-cpp", "tracy-profiler")

	add_files("**.cpp")
	add_tests("default")
target_end()
