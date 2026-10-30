package("fastgltf")
	set_homepage("https://fastgltf.readthedocs.io/")
	set_description("A modern C++17 glTF 2.0 library focused on speed, correctness, and usability")
	set_license("MIT")

	add_urls("https://github.com/spnda/fastgltf.git")

	add_deps("cmake")
	add_deps("simdjson")

	on_install("windows|x64", function (package)
		local configs =
		{
			"-DFASTGLTF_USE_CUSTOM_SMALLVECTOR=OFF",
			"-DFASTGLTF_DISABLE_CUSTOM_MEMORY_POOL=OFF",
			"-DFASTGLTF_USE_64BIT_FLOAT=OFF",
			"-DFASTGLTF_COMPILE_AS_CPP20=ON",
		}
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
		import("package.tools.cmake").install(package, configs, { packagedeps = "simdjson" })
	end)

	on_test(function (package)
		assert(package:has_cxxtypes("fastgltf::Parser", { configs = { languages = "c++20" }, includes = "fastgltf/core.hpp" }))
	end)
package_end()
