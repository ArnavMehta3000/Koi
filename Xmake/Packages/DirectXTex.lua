package("directxtex")
	set_homepage("https://walbourn.github.io/directxtex/")
	set_description("DirectXTex texture processing library")
	set_license("MIT")

	add_urls("https://github.com/microsoft/DirectXTex.git")

	add_deps("cmake")

	-- Koi's importers need the D3D11 GPU compressor and the D3D12 helpers; OpenMP speeds up the CPU codecs
	on_install("windows|x64", function (package)
		local configs =
		{
			"-DBUILD_TOOLS=OFF",
			"-DBUILD_SAMPLE=OFF",
			"-DBUILD_DX11=ON",
			"-DBUILD_DX12=ON",
			"-DBC_USE_OPENMP=ON",
			"-DENABLE_SPECTRE_MITIGATION=OFF",
			"-DDISABLE_MSVC_ITERATOR_DEBUGGING=OFF",
			"-DENABLE_CODE_ANALYSIS=OFF",
			"-DUSE_PREBUILT_SHADERS=OFF",
			"-DENABLE_OPENEXR_SUPPORT=OFF",
		}
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			#include <DirectXTex.h>
			void test() {
				DirectX::IsValid(DXGI_FORMAT_UNKNOWN);
			}
		]]}, { configs = { languages = "c++17" } }))
	end)
package_end()
