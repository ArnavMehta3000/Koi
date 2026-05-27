package("hlslppmath")  -- Using name hlslppmath instead of hlslpp to prevent xmake package conflict
	set_kind("library", { headeronly = true })
	set_homepage("https://github.com/redorav/hlslpp")
    set_description("Math library using hlsl syntax with SSE/NEON support")
    set_license("MIT")

    add_urls("https://github.com/redorav/hlslpp/archive/refs/tags/$(version).zip",
                 "https://github.com/redorav/hlslpp.git")

    add_versions("3.9", "321f37b392f0e7e50ef983c4109313f5476d7d7c89f038dd501966117d27c286")

    on_install("windows", function (package)
	        os.cp("include", package:installdir())
    end)

    on_test(function (package)
	    assert(package:check_cxxsnippets({test = [[
	        void test() {
	            using namespace hlslpp;

	            float4 foo4 = float4(1, 2, 3, 4);
	            float4x4 fooMatrix4x4 = float4x4( 1, 2, 3, 4,
	                                5, 6, 7, 8,
	                                8, 7, 6, 5,
	                                4, 3, 2, 1);
	            float4 myTransformedVector = mul(fooMatrix4x4, foo4);
	        }
	    ]]}, {configs = {languages = "c++14"}, includes = "hlsl++.h"}))
	end)
package_end()

add_requires("hlslppmath")
