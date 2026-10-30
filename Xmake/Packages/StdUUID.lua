package("stduuid")
	set_kind("library", { headeronly = true })
	set_homepage("https://github.com/mariusbancila/stduuid")
	set_description("A C++17 cross-platform implementation for UUIDs")
	set_license("MIT")

	add_urls("https://github.com/mariusbancila/stduuid.git")

	-- uuid.h picks std::span on its own under C++20, so the gsl dependency is dropped
	on_install(function (package)
		os.cp("include/uuid.h", package:installdir("include/stduuid"))
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			void test() {
				uuids::uuid empty;
			}
		]]}, { configs = { languages = "c++20" }, includes = "stduuid/uuid.h" }))
	end)
package_end()
