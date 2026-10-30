package("box3d")
	set_homepage("https://github.com/erincatto/box3d")
	set_description("Box3D is a 3D physics engine for games")
	set_license("MIT")

	add_urls("https://github.com/erincatto/box3d.git")

	add_deps("cmake")

	add_configs("simd", { description = "Enable SIMD math", default = true, type = "boolean" })

	on_install(function (package)
		local configs =
		{
			"-DBOX3D_SAMPLES=OFF",
			"-DBOX3D_UNIT_TESTS=OFF",
			"-DBOX3D_BENCHMARKS=OFF",
			"-DBOX3D_DOCS=OFF",
		}
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
		table.insert(configs, "-DBOX3D_DISABLE_SIMD=" .. (package:config("simd") and "OFF" or "ON"))
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:check_csnippets({ test = [[
			void test(void) {
				b3WorldDef worldDef = b3DefaultWorldDef();
				b3WorldId worldId = b3CreateWorld(&worldDef);
				b3DestroyWorld(worldId);
			}
		]]}, { configs = { languages = "c17" }, includes = "box3d/box3d.h" }))
	end)
package_end()
