package("dxc")
	set_homepage("https://github.com/microsoft/DirectXShaderCompiler/")
    set_description("DirectX Shader Compiler")
    set_license("LLVM")

    -- Manually using the latest version
    add_urls("https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip")
    add_versions("1.9.2602", "a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e")

    on_install("windows|x64", function (package)
        os.cp("bin/x64/*", package:installdir("bin"))
        os.cp("inc/*", package:installdir("include"))
        os.cp("lib/x64/*", package:installdir("lib"))
        package:addenv("PATH", "bin")
    end)

    on_test(function (package)
        os.vrun("dxc -help")
        if package:is_plat("windows") then
            assert(package:has_cxxfuncs("DxcCreateInstance", { includes = {"windows.h", "dxcapi.h" } }))
        elseif package:is_plat("linux") then
            assert(package:has_cxxfuncs("DxcCreateInstance", { includes = {"dxc/dxcapi.h" } }))
        end
    end)
package_end()
