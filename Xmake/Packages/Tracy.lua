package("tracy-profiler")
	set_homepage("https://github.com/wolfpld/tracy")
	set_description("C++ frame profiler")
	set_license("BSD-3-Clause")

	add_urls("https://github.com/wolfpld/tracy.git")

	add_deps("cmake")
	add_includedirs("include", "include/tracy")
	add_syslinks("ws2_32", "dbghelp")

	-- The only options Koi changes from Tracy's defaults; the client must see the same defines
	local kOptions = { "TRACY_CALLSTACK", "TRACY_NO_CALLSTACK_INLINES", "TRACY_FIBERS" }
	add_defines(table.unpack(kOptions))

	on_install("windows", function (package)
		io.replace("CMakeLists.txt", [[/$<IF:$<CONFIG:Release>,,$<CONFIG>>]], "", { plain = true })

		local configs = {}
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=OFF")
		for _, option in ipairs(kOptions) do
			table.insert(configs, "-D" .. option .. "=ON")
		end
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			#define TRACY_ENABLE
			#include <tracy/Tracy.hpp>
			void test() {
				TracyFiberEnter("Fiber");
			}
		]]}, { configs = { languages = "c++14" } }))
	end)
package_end()
