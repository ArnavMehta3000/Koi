target("ImGui")  -- Custom ImGui package
	set_kind("object")
	set_group("External")

	add_defines([[IMGUI_USER_CONFIG="KoiImConfig.h"]]          , { public = true })

	add_includedirs("$(projectdir)/External/ImGui"             , { public = true })
	add_includedirs("$(projectdir)/External/ImGuiConfig"       , { public = true })
	add_includedirs("$(projectdir)/External/ImGui/misc/cpp"    , { public = true })
	add_includedirs("$(projectdir)/External/IconFontCppHeaders", { public = true })  -- Font Awesome 6 ICON_FA_* names

	add_files("$(projectdir)/External/ImGui/**.cpp"            , { public = true })
	add_headerfiles("$(projectdir)/External/ImGui/**.h"        , { public = true })

	add_syslinks("imm32")

	remove_files("$(projectdir)/External/ImGui/misc/fonts/binary_to_compressed_c.cpp")
target_end()

target("ImGuizmo")
	set_kind("object")
	set_group("External")
	add_deps("ImGui", { public = true })
	add_includedirs("$(projectdir)/External/ImGuizmo", { public = true })
	add_files("$(projectdir)/External/ImGuizmo/**.cpp", { public = true })
	add_headerfiles("$(projectdir)/External/ImGuizmo/**.h", { public = true })
target_end()
