package("tracy-profiler")
	set_homepage("https://github.com/wolfpld/tracy")
	set_description("C++ frame profiler")
	set_license("BSD-3-Clause")

	add_urls("https://github.com/wolfpld/tracy.git")

	add_deps("cmake")
	add_includedirs("include", "include/tracy")
	add_syslinks("ws2_32", "dbghelp")

	-- The client is always built enabled; Koi defines TRACY_ENABLE itself, in profile modes only
	on_install("windows", function (package)
		io.replace("CMakeLists.txt", [[/$<IF:$<CONFIG:Release>,,$<CONFIG>>]], "", { plain = true })

		local configs = { "-DTRACY_ENABLE=ON", "-DBUILD_SHARED_LIBS=OFF" }
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			#define TRACY_ENABLE
			#include <tracy/Tracy.hpp>
			void test() {
				FrameMark;
			}
		]]}, { configs = { languages = "c++17" } }))
	end)
package_end()
