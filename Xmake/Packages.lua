-- Custom packages
includes("Packages/Kirin.lua")
includes("Packages/HLSLPP.lua")

add_requires(
    "Kirin",
    "glfw",
    "directx-headers",
    "myhlslpp"
)


add_requires("fastgltf", { configs = { cxx_standard = "20" }})
add_requires("spdlog", { configs = { wchar = true, std_format = false, fmt_external = false }})
