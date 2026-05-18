-- Custom packages
includes("Packages/HLSLPP.lua")

add_requires(
    "glfw",
    "directx-headers"
)

add_requires("toml++ f22f035fe2d63e2be3d266d8f100e7812a9ab9bd")
add_requires("tracy eb411b1138eeab0f02dd3ca7bcbc051aa4436f2f")
add_requires("fastgltf", { configs = { cxx_standard = "20" }})
add_requires("spdlog", { configs = { wchar = true, std_format = false, fmt_external = false }})
