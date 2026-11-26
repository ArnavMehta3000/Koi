package("winpixevent")
	set_homepage("https://devblogs.microsoft.com/pix/winpixeventruntime/")
	set_description("WinPixEventRuntime, the prebuilt NuGet headers + DLL")
	set_license("MIT")

	set_urls("https://www.nuget.org/api/v2/package/WinPixEventRuntime/$(version)/#WinPixEventRuntime-$(version).zip")
	add_versions("1.0.240308001", "726acc93d6968e2146261a1e415521747d50ad69894c2b42b5d0d4c29fd66ec4")

	add_links("WinPixEventRuntime")

	on_install("windows|x64", function (package)
		os.cp("Include/*", package:installdir("include"))
		os.cp("bin/x64/WinPixEventRuntime.lib", package:installdir("lib"))
		os.cp("bin/x64/WinPixEventRuntime.dll", package:installdir("bin"))
		package:addenv("PATH", "bin")
	end)

	on_test(function (package)
		assert(package:has_cxxfuncs("PIXEndCapture", { configs = { languages = "c++17" }, includes = { "windows.h", "WinPixEventRuntime/pix3.h" } }))
	end)
package_end()
