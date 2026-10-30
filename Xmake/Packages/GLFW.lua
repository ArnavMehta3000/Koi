package("glfw")
	set_homepage("https://www.glfw.org/")
	set_description("GLFW is an Open Source, multi-platform library for OpenGL, OpenGL ES and Vulkan application development.")
	set_license("zlib")

	add_urls("https://github.com/glfw/glfw.git")

	add_deps("cmake")
	add_defines("GLFW_INCLUDE_NONE")
	add_syslinks("user32", "shell32", "gdi32")

	on_install("windows", function (package)
		local configs = { "-DGLFW_BUILD_DOCS=OFF", "-DGLFW_BUILD_TESTS=OFF", "-DGLFW_BUILD_EXAMPLES=OFF" }
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=OFF")
		table.insert(configs, "-DUSE_MSVC_RUNTIME_LIBRARY_DLL=" .. (package:has_runtime("MT") and "OFF" or "ON"))
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:check_csnippets({ test = [[
			void test() {
				glfwInit();
				glfwTerminate();
			}
		]]}, { configs = { languages = "c11" }, includes = "GLFW/glfw3.h" }))
	end)
package_end()
