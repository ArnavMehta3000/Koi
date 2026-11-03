package("doctest")
	set_kind("library", { headeronly = true })
	set_homepage("https://github.com/doctest/doctest")
	set_description("The fastest feature-rich C++11/14/17/20/23 single-header testing framework")
	set_license("MIT")

	add_urls("https://github.com/doctest/doctest.git")

	on_install(function (package)
		os.cp("doctest/doctest.h", package:installdir("include/doctest"))
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			void test() { doctest::Context context; }
		]]}, { configs = { languages = "c++17" }, includes = "doctest/doctest.h" }))
	end)
package_end()
