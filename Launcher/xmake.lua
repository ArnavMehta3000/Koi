local _we = get_config("WithEditor")
local _with_editor = (_we == 1 or _we == "1" or _we == true)

-- Only one launcher exists per build configuration. The `KoiGame` rule
-- routes the game target to whichever launcher is active
if not _with_editor then

	-- Runtime/Standalone build
	target("koi-runtime")
		set_kind("binary")
		add_files("Runtime/RuntimeMain.cpp")
		add_deps("Koi.Engine")

		if not is_mode("debug", "releasedbg") then
			add_rules("win.sdk.application")
		end
	target_end()

else

	-- Editor build
	target("koi-editor")
		set_kind("binary")
		add_files("Editor/EditorMain.cpp")
		add_deps("Koi.Engine", "Koi.Editor")

		if not is_mode("debug", "releasedbg") then
			add_rules("win.sdk.application")
		end
	target_end()

end
