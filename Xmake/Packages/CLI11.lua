package("cli11")
	set_kind("library", { headeronly = true })
	set_homepage("https://github.com/CLIUtils/CLI11")
	set_description("CLI11 is a command line parser for C++11 and beyond that provides a rich feature set with a simple and intuitive interface.")
	set_license("BSD")

	add_urls("https://github.com/CLIUtils/CLI11.git")

	add_syslinks("shell32")

	on_install(function (package)
		os.cp("include", package:installdir())
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			CLI::App app{"Test", "test"};
		]]}, { configs = { languages = "cxx11" }, includes = "CLI/CLI.hpp" }))
	end)
package_end()
