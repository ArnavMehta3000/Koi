target("ImGui")  -- Custom ImGui package
	set_kind("object")
	set_group("External")

	add_includedirs("$(projectdir)/External/ImGui", { public = true })
	add_includedirs("$(projectdir)/External/ImGui/misc/cpp", { public = true })
	add_files("$(projectdir)/External/ImGui/**.cpp", { public = true })
	add_headerfiles("$(projectdir)/External/ImGui/**.h", { public = true })

	add_packages("glfw")
	add_syslinks("imm32")

	-- Standalone CLI tool with its own main() - everything else under misc/ is fair game
	-- (imgui_stdlib.cpp in particular, which ImGui::InputText(std::string*) needs)
	remove_files("$(projectdir)/External/ImGui/misc/fonts/binary_to_compressed_c.cpp")
target_end()

target("ImGuiColorTextEdit")
	set_kind("object")
	set_group("External")
	add_deps("ImGui", { public = true })
	add_includedirs("$(projectdir)/External/ImGuiColorTextEdit", { public = true })
	add_files("$(projectdir)/External/ImGuiColorTextEdit/**.cpp", { public = true })
	add_headerfiles("$(projectdir)/External/ImGuiColorTextEdit/**.h", { public = true })
target_end()

target("ImGuizmo")
	set_kind("object")
	set_group("External")
	add_deps("ImGui", { public = true })
	add_includedirs("$(projectdir)/External/ImGuizmo", { public = true })
	add_files("$(projectdir)/External/ImGuizmo/**.cpp", { public = true })
	add_headerfiles("$(projectdir)/External/ImGuizmo/**.h", { public = true })
target_end()
