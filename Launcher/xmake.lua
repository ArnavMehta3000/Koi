target("koi-runtime")

	set_kind("binary")
    add_files("Runtime/Main.cpp")

	if not is_mode("debug", "releasedbg") then
		add_rules("win.sdk.application")
	end

    add_deps("Koi.Engine", "Sandbox")
target_end()
