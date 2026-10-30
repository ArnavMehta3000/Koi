package("stb")
	set_kind("library", { headeronly = true })
	set_homepage("https://github.com/nothings/stb")
	set_description("single-file public domain (or MIT licensed) libraries for C/C++")

	add_urls("https://github.com/nothings/stb.git")

	add_includedirs("include", "include/stb")

	on_install(function (package)
		os.cp("*.h", package:installdir("include/stb"))
		os.cp("*.c", package:installdir("include/stb"))
	end)

	on_test(function (package)
		assert(package:has_cfuncs("stbi_load_from_memory", { includes = "stb/stb_image.h" }))
	end)
package_end()
