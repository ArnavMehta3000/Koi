package("Flecs")
	set_homepage("https://github.com/SanderMertens/flecs")
	set_description("A fast entity component system (ECS) for C & C++")
	set_license("MIT")

	add_urls("https://github.com/SanderMertens/flecs.git")

	add_deps("cmake")
	add_defines("flecs_STATIC")
	add_syslinks("wsock32", "ws2_32", "Dbghelp")

	on_install("windows", function (package)
		local configs = { "-DFLECS_STATIC=ON", "-DFLECS_SHARED=OFF" }
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DFLECS_PIC=" .. (package:config("pic") and "ON" or "OFF"))
		import("package.tools.cmake").install(package, configs)

		os.trycp(path.join(package:builddir(), "flecs.pdb"), package:installdir("lib"))
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			void test() {
				flecs::world ecs;
			}
		]]}, { configs = { languages = "c++17" }, includes = "flecs.h" }))
	end)
package_end()
