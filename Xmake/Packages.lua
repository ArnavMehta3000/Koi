-- Koi's own targets
includes("Packages/SimpleMath.lua")
includes("Packages/GameInput.lua")
includes("Packages/ImGui.lua")
includes("Packages/KoiProfiler.lua")

-- Package recipes, adapted from xmake-repo. All but dxc are git-only
includes("Packages/Box3D.lua")
includes("Packages/CLI11.lua")
includes("Packages/DirectXHeaders.lua")
includes("Packages/DirectXTex.lua")
includes("Packages/dxc.lua")
includes("Packages/FastGLTF.lua")
includes("Packages/SimdJson.lua")
includes("Packages/SimdUtf.lua")
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
add_requires("fastgltf 87cfbceeeda127ccc9092694279437b700046119")        -- v1.0.1
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
add_requireconfs("fastgltf.simdjson",  { override = true, version = "ec73615105bd405fc36ddcd1b4a6afb045aeffc1" })  -- v5.1.0
add_requireconfs("fastgltf.simdutf",   { override = true, version = "59589cd3df6823cbfdbd253cde3377e7c3e1fe9b" })  -- v9.3.3
