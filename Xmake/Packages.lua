-- Custom packages
includes("Packages/hlslpp.lua")
includes("Packages/stb.lua")

add_requires(
    "glfw",
    "directx-headers",
    "cli11"
)

add_requires("toml++ f22f035fe2d63e2be3d266d8f100e7812a9ab9bd")
add_requires("tracy b81ef061f8e68a2fd104a2fb6e3362b96b62d6c9", {
    configs = { tracy_enable = is_mode("profile") }
})
add_requires("fastgltf", { configs = { cxx_standard = "20" }})
add_requires("spdlog", { configs = { wchar = true, std_format = false, fmt_external = false }})
