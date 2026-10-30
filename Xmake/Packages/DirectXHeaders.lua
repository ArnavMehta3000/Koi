package("directx-headers")
	set_homepage("https://github.com/microsoft/DirectX-Headers")
	set_description("Official DirectX headers")
	set_license("MIT")

	add_urls("https://github.com/microsoft/DirectX-Headers.git")

	add_deps("cmake")

	on_install("windows", function (package)
		local configs = { "-DDXHEADERS_BUILD_TEST=OFF", "-DDXHEADERS_BUILD_GOOGLE_TEST=OFF" }
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:has_cxxtypes("CD3DX12FeatureSupport", { configs = { languages = "cxx14" }, includes = "directx/d3dx12.h" }))
	end)
package_end()
