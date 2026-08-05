-- Custom packages
includes("Packages/SimpleMath.lua")
includes("Packages/Tracy.lua")
includes("Packages/dxc.lua")
includes("Packages/ImGui.lua")
includes("Packages/AngelScript.lua")
includes("Packages/Flecs.lua")

add_requires(
    "glfw",
    "directx-headers",
    "cli11",
    "dxc", -- custom package
	"Flecs" -- custom package
)

add_requires("tracy-profiler", { configs = { tracy_enable = is_mode("profile") } })  -- custom package
add_requires("stb 31c1ad37456438565541f4919958214b6e762fb4")
add_requires("toml++ f22f035fe2d63e2be3d266d8f100e7812a9ab9bd")
add_requires("fastgltf", { configs = { cxx_standard = "20" }})
add_requires("spdlog", { configs = { wchar = true, std_format = false, fmt_external = false }})
