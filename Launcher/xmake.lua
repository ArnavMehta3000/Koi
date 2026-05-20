target("koi-runtime")

	set_kind("binary")
    add_files("Runtime/Main.cpp")

	if not is_mode("debug", "releasedbg") then
		add_rules("win.sdk.application")
	end

add_deps("Koi.Engine", "Sandbox")

on_config(function(target)
    import("core.project.project")
    local project_dir = project.target("Sandbox"):scriptdir()
    local project_file = path.join(project_dir, "Sandbox.toml")

    target:set("runargs", "-p", project_file)
end)
target_end()
