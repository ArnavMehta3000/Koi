package("simdutf")
	set_homepage("https://simdutf.github.io/simdutf/")
	set_description("Unicode routines (UTF8, UTF16, UTF32) and Base64: billions of characters per second using SSE2, AVX2, NEON, AVX-512, RISC-V Vector Extension, LoongArch64.")
	set_license("Apache-2.0")

	add_urls("https://github.com/simdutf/simdutf.git")

	add_deps("cmake")

	on_install(function (package)
		local configs =
		{
			"-DSIMDUTF_TESTS=OFF",
			"-DSIMDUTF_BENCHMARKS=OFF",
			"-DSIMDUTF_TOOLS=OFF",
			"-DSIMDUTF_ICONV=OFF",
		}
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:has_cxxfuncs("simdutf::base64_length_from_binary(0)", { configs = { languages = "c++17" }, includes = "simdutf.h" }))
	end)
package_end()
