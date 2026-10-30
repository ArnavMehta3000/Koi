package("tracy-tools")  -- Tracy's prebuilt viewer and command-line tools; the version must match tracy-profiler's
	set_kind("binary")
	set_homepage("https://github.com/wolfpld/tracy")
	set_description("Tracy profiler viewer, capture, export and import tools")
	set_license("BSD-3-Clause")

	add_urls("https://github.com/wolfpld/tracy/releases/download/v$(version)/windows-$(version).zip")
	add_versions("0.14.1", "f7499d74914aa3ba94a2c1ce72f36477d7b61d9d0f7c9790e05274c258c97fb5")

	on_install("windows|x64", function (package)
		os.cp("*.exe", package:installdir("bin"))
	end)

	on_test(function (package)
		assert(os.isfile(path.join(package:installdir("bin"), "tracy-profiler.exe")))
	end)
package_end()
