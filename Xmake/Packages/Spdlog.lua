package("spdlog")
	set_kind("library", { headeronly = true })
	set_homepage("https://github.com/gabime/spdlog")
	set_description("Fast C++ logging library.")
	set_license("MIT")

	add_urls("https://github.com/gabime/spdlog.git")

	-- Header-only with the bundled fmt, which needs /utf-8 under MSVC
	add_defines("SPDLOG_WCHAR_TO_UTF8_SUPPORT")
	add_cxxflags("/utf-8")

	on_install(function (package)
		os.cp("include", package:installdir())
	end)

	on_test(function (package)
		assert(package:has_cxxfuncs("spdlog::info(\"\")", { includes = "spdlog/spdlog.h", configs = { languages = "c++14" } }))
	end)
package_end()
