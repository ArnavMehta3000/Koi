package("ctre")
	set_kind("library", { headeronly = true })
	set_homepage("https://github.com/hanickadot/compile-time-regular-expressions/")
	set_description("ctre is a Compile time PCRE (almost) compatible regular expression matcher.")
	set_license("Apache-2.0")

	add_urls("https://github.com/hanickadot/compile-time-regular-expressions.git")

	add_deps("cmake")

	-- CMake install, not a header copy: reflect-cpp finds it through the exported config
	on_install(function (package)
		import("package.tools.cmake").install(package, { "-DCTRE_BUILD_TESTS=OFF" })
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			#include <ctre.hpp>
			static void test() {
				static constexpr auto pattern = ctll::fixed_string{R"(\s+)"};
				bool match = ctre::match<pattern>(" ");
			}
		]]}, { configs = { languages = "c++20" } }))
	end)
package_end()
