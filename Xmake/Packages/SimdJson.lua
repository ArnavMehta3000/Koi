package("simdjson")
	set_homepage("https://simdjson.org")
	set_description("Ridiculously fast JSON parsing, UTF-8 validation and JSON minifying for popular 64 bit systems.")
	set_license("Apache-2.0")

	add_urls("https://github.com/simdjson/simdjson.git")

	add_deps("cmake")

	on_install(function (package)
		local configs =
		{
			"-DSIMDJSON_JUST_LIBRARY=ON",
			"-DSIMDJSON_SANITIZE_UNDEFINED=OFF",
			"-DSIMDJSON_SANITIZE=OFF",
			"-DSIMDJSON_SANITIZE_THREADS=OFF",
			"-DSIMDJSON_BUILD_STATIC=ON",
			"-DSIMDJSON_DEVELOPMENT_CHECKS=OFF",
			"-DSIMDJSON_VERBOSE_LOGGING=OFF",
			"-DSIMDJSON_EXCEPTIONS=ON",
			"-DSIMDJSON_ENABLE_THREADS=ON",
		}
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			void test() {
				simdjson::dom::parser parser;
			}
		]]}, { configs = { languages = "c++17" }, includes = "simdjson.h" }))
	end)
package_end()
