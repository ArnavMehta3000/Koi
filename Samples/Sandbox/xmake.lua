target("Sandbox")
	add_rules("KoiGame")
	set_default(false)  -- samples build on request: `xmake build Sandbox`, `xmake run Sandbox`
	add_files("Source/**.cpp")
	add_headerfiles("Source/**.h")
target_end()
