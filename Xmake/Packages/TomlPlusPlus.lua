package("toml++")
	set_kind("library", { headeronly = true })
	set_homepage("https://marzer.github.io/tomlplusplus/")
	set_description("toml++ is a header-only TOML config file parser and serializer for C++17 (and later!).")
	set_license("MIT")

	add_urls("https://github.com/marzer/tomlplusplus.git")

	add_deps("cmake")

	-- CMake install, not a header copy: reflect-cpp finds it through the exported config
	on_install(function (package)
		import("package.tools.cmake").install(package)
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			#include <toml++/toml.h>
			using namespace std::string_view_literals;
			static void test() {
				auto tbl = toml::parse("name = \"toml++\""sv);
			}
		]]}, { configs = { languages = "c++17" } }))
	end)
package_end()
