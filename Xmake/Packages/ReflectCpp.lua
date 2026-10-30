package("reflect-cpp")
	set_homepage("https://github.com/getml/reflect-cpp")
	set_description("A C++20 library for fast serialization, deserialization and validation using reflection.")
	set_license("MIT")

	add_urls("https://github.com/getml/reflect-cpp.git", { submodules = false })

	-- TOML is the only format Koi reads
	add_deps("cmake", "toml++", "ctre")

	on_install("windows", function (package)
		local configs =
		{
			"-DREFLECTCPP_USE_BUNDLED_DEPENDENCIES=OFF",
			"-DREFLECTCPP_USE_VCPKG=OFF",
			"-DREFLECTCPP_JSON=OFF",
			"-DREFLECTCPP_BSON=OFF",
			"-DREFLECTCPP_CBOR=OFF",
			"-DREFLECTCPP_CAPNPROTO=OFF",
			"-DREFLECTCPP_FLEXBUFFERS=OFF",
			"-DREFLECTCPP_MSGPACK=OFF",
			"-DREFLECTCPP_XML=OFF",
			"-DREFLECTCPP_TOML=ON",
			"-DREFLECTCPP_UBJSON=OFF",
			"-DREFLECTCPP_YAML=OFF",
		}
		table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
		table.insert(configs, "-DBUILD_SHARED_LIBS=OFF")
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function (package)
		assert(package:check_cxxsnippets({ test = [[
			struct Person {
				std::string first_name;
				int age;
			};
			void test() {
				const auto first_name = rfl::get<"first_name">(rfl::to_named_tuple(Person{ "Homer", 45 }));
			}
		]]}, { configs = { languages = "c++20" }, includes = "rfl.hpp" }))
	end)
package_end()
