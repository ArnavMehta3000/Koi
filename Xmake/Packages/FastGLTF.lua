package("fastgltf")
	set_homepage("https://fastgltf.readthedocs.io/")
	set_description("A modern C++20 glTF 2.0 library focused on speed, correctness, and usability")
	set_license("MIT")

	add_urls("https://github.com/spnda/fastgltf.git")

	add_deps("cmake")
	add_deps("simdjson", "simdutf")

	on_install("windows|x64", function (package)
		local configs =
		{
			"-DFASTGLTF_USE_64BIT_FLOAT=OFF",
			"-DFASTGLTF_DOWNLOAD_SIMDJSON=OFF",
			"-DFASTGLTF_DOWNLOAD_SIMDUTF=OFF",
		}
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
		import("package.tools.cmake").install(package, configs, { packagedeps = { "simdjson", "simdutf" } })
	end)

	on_test(function (package)
		assert(package:has_cxxtypes("fastgltf::Parser", { configs = { languages = "c++20" }, includes = "fastgltf/core.hpp" }))
	end)
package_end()
