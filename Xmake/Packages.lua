-- Koi's own targets
includes("Packages/SimpleMath.lua")
includes("Packages/ImGui.lua")
includes("Packages/KoiProfiler.lua")

-- Package recipes, adapted from xmake-repo. All but dxc are git-only
includes("Packages/Box3D.lua")
includes("Packages/CLI11.lua")
includes("Packages/CTRE.lua")
includes("Packages/DirectXHeaders.lua")
includes("Packages/DirectXTex.lua")
includes("Packages/dxc.lua")
includes("Packages/FastGLTF.lua")
includes("Packages/Flecs.lua")
includes("Packages/GLFW.lua")
includes("Packages/ReflectCpp.lua")
includes("Packages/SimdJson.lua")
includes("Packages/Spdlog.lua")
includes("Packages/StdUUID.lua")
includes("Packages/STB.lua")
includes("Packages/TomlPlusPlus.lua")
includes("Packages/Tracy.lua")
includes("Packages/TracyTools.lua")

local kTomlPlusPlus = "f22f035fe2d63e2be3d266d8f100e7812a9ab9bd"

add_requires("box3d 336263320469e5e9febfe18697ab562488f7e97a")           -- main
add_requires("cli11 cbd58a3696887b34c70949aef21a71735a0c2ad5")           -- v2.7.2
add_requires("directx-headers ee479f0bd5f7b884f202bcf0c3f076cc050dd256") -- v1.619.5
add_requires("directxtex 32b2a8ef351ab08f822f6c0937fa37e07484714a")      -- jul2025, editor-only
add_requires("dxc 1.9.2602")                                             -- release binaries
add_requires("fastgltf 0d1b67a28c4950ea2deb796702006dcbe31e02b3")        -- v0.9.0
add_requires("Flecs fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8")           -- v4.1.6
add_requires("glfw 7b6aead9fb88b3623e3b3725ebb42670cbe4c579")            -- 3.4
add_requires("reflect-cpp 1327ef1d0b403f1424776da4c47bf0c7a6b0feaa")     -- v0.25.0
add_requires("spdlog 79524ddd08a4ec981b7fea76afd08ee05f83755d")          -- v1.17.0
add_requires("stb 31c1ad37456438565541f4919958214b6e762fb4")
add_requires("stduuid 3afe7193facd5d674de709fccc44d5055e144d7a")         -- v1.2.3
add_requires("toml++ " .. kTomlPlusPlus)
add_requires("tracy-profiler 30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c")  -- v0.14.1

-- The viewer must speak the client's protocol, so bump both together. `xmake koi tracy` launches it
if is_mode(".*-profile") then
	add_requires("tracy-tools 0.14.1")
end

-- Dependencies of the packages above
add_requireconfs("fastgltf.simdjson",  { override = true, version = "f5de14f09256982933af2849beb43778bd421ca7" })  -- v4.6.11
add_requireconfs("reflect-cpp.ctre",   { override = true, version = "e34c26ba149b9fd9c34aa0f678e39739641a0d1e" })  -- v3.10.0
add_requireconfs("reflect-cpp.toml++", { override = true, version = kTomlPlusPlus })
