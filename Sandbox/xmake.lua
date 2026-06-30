-- Need to 'cook assets' in editor before being able to run in standalone mode
-- Doing this so that there is no need to precompile shaders, copy assets, etc.

target("Sandbox")
	add_rules("KoiGame")

	add_includedirs("Scripts")
	add_files("Scripts/**.ixx", "Scripts/**.cpp")
	add_headerfiles("Scripts/**.h")

	-- Use project:// to refer to these assets
	add_extrafiles("Assets/Shaders/**.hlsl")
	add_extrafiles("Assets/Models/**.gltf")
	add_extrafiles("Assets/Scenes/**.toml")
	add_extrafiles("Sandbox.toml")
target_end()
