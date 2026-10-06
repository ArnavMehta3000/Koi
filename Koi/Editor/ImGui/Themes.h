#pragma once
#include "Koi/Engine/Core/Types.h"
#include <imgui.h>
#include <array>

namespace koi::editor
{
	enum class Theme : u8
	{
		Dracula,
		CatppuccinMocha,
		GruvboxHard,
		MAX_COUNT
	};

	struct ThemeEntry
	{
		const char* Name;
		void      (*Apply)();
	};

	namespace utils
	{
		inline void SetupImGuiDraculaStyle()
		{
		    ImGuiStyle& style = ImGui::GetStyle();
		    ImVec4* colors = style.Colors;

		    style.WindowPadding     = ImVec2(10.0f, 10.0f);
		    style.FramePadding      = ImVec2(6.0f, 4.0f);
		    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
		    style.ScrollbarSize     = 14.0f;
		    style.GrabMinSize       = 12.0f;
		    style.WindowRounding    = 6.0f;
		    style.FrameRounding     = 4.0f;
		    style.PopupRounding     = 4.0f;
		    style.ScrollbarRounding = 12.0f;
		    style.GrabRounding      = 4.0f;
		    style.TabRounding       = 4.0f;
		    style.WindowBorderSize  = 1.0f;
		    style.FrameBorderSize   = 1.0f;

			colors[ImGuiCol_Text]                 = ImVec4(0.97f, 0.97f, 0.95f, 1.00f); // #f8f8f2
		    colors[ImGuiCol_TextDisabled]         = ImVec4(0.38f, 0.45f, 0.64f, 1.00f); // #6272a4
		    colors[ImGuiCol_WindowBg]             = ImVec4(0.16f, 0.16f, 0.21f, 1.00f); // #282a36
		    colors[ImGuiCol_ChildBg]              = ImVec4(0.16f, 0.16f, 0.21f, 0.00f);
		    colors[ImGuiCol_PopupBg]              = ImVec4(0.16f, 0.16f, 0.21f, 0.96f);
		    colors[ImGuiCol_Border]               = ImVec4(0.27f, 0.28f, 0.35f, 1.00f); // #44475a
		    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		    colors[ImGuiCol_FrameBg]              = ImVec4(0.27f, 0.28f, 0.35f, 1.00f); // #44475a
		    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.38f, 0.45f, 0.64f, 1.00f); // #6272a4
		    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);
		    colors[ImGuiCol_TitleBg]              = ImVec4(0.13f, 0.14f, 0.18f, 1.00f); // Darker
		    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
		    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
		    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
		    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);
		    colors[ImGuiCol_CheckMark]            = ImVec4(0.31f, 0.98f, 0.48f, 1.00f); // #50fa7b (Green)
		    colors[ImGuiCol_SliderGrab]           = ImVec4(0.74f, 0.58f, 0.98f, 1.00f); // #bd93f9 (Purple)
		    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.84f, 0.68f, 1.00f, 1.00f);
		    colors[ImGuiCol_Button]               = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_ButtonHovered]        = ImVec4(1.00f, 0.47f, 0.78f, 1.00f); // #ff79c6 (Pink)
		    colors[ImGuiCol_ButtonActive]         = ImVec4(0.80f, 0.37f, 0.62f, 1.00f);
		    colors[ImGuiCol_Header]               = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
		    colors[ImGuiCol_HeaderActive]         = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);
		    colors[ImGuiCol_Tab]                  = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
		    colors[ImGuiCol_TabHovered]           = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_TabSelected]          = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_TabDimmed]            = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
		    colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
		    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
		    colors[ImGuiCol_TableBorderLight]     = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_PlotLines]            = ImVec4(0.55f, 0.91f, 0.99f, 1.00f); // #8be9fd (Cyan)
		    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
		    colors[ImGuiCol_NavCursor]            = ImVec4(0.74f, 0.58f, 0.98f, 1.00f);
		    colors[ImGuiCol_DockingPreview]       = ImVec4(0.74f, 0.58f, 0.98f, 0.50f);
		    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
		}

		inline void SetupImGuiCatppuccinMochaStyle()
		{
		    ImGuiStyle& style = ImGui::GetStyle();
		    ImVec4* colors = style.Colors;

		    style.WindowPadding     = ImVec2(12.0f, 12.0f);
		    style.FramePadding      = ImVec2(6.0f, 4.0f);
		    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
		    style.ScrollbarSize     = 14.0f;
		    style.GrabMinSize       = 12.0f;
		    style.WindowRounding    = 8.0f;
		    style.FrameRounding     = 5.0f;
		    style.PopupRounding     = 5.0f;
		    style.ScrollbarRounding = 12.0f;
		    style.GrabRounding      = 5.0f;
		    style.TabRounding       = 5.0f;
		    style.WindowBorderSize  = 1.0f;
		    style.FrameBorderSize   = 0.0f;
		    style.PopupBorderSize   = 1.0f;

		    colors[ImGuiCol_Text]                 = ImVec4(0.80f, 0.84f, 0.96f, 1.00f); // Text
		    colors[ImGuiCol_TextDisabled]         = ImVec4(0.42f, 0.45f, 0.55f, 1.00f); // Surface1
		    colors[ImGuiCol_WindowBg]             = ImVec4(0.12f, 0.12f, 0.18f, 1.00f); // Base
		    colors[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.09f, 0.15f, 1.00f); // Mantle
		    colors[ImGuiCol_PopupBg]              = ImVec4(0.07f, 0.07f, 0.11f, 0.96f); // Crust
		    colors[ImGuiCol_Border]               = ImVec4(0.19f, 0.20f, 0.27f, 1.00f); // Surface0
		    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		    colors[ImGuiCol_FrameBg]              = ImVec4(0.19f, 0.20f, 0.27f, 1.00f); // Surface0
		    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.25f, 0.26f, 0.35f, 1.00f); // Surface1
		    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.31f, 0.32f, 0.42f, 1.00f); // Surface2
		    colors[ImGuiCol_TitleBg]              = ImVec4(0.09f, 0.09f, 0.15f, 1.00f); // Mantle
		    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.12f, 0.18f, 1.00f); // Base
		    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.07f, 0.07f, 0.11f, 1.00f); // Crust
		    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
		    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.31f, 0.32f, 0.42f, 1.00f); // Surface2
		    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.37f, 0.38f, 0.51f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.42f, 0.45f, 0.55f, 1.00f);
		    colors[ImGuiCol_CheckMark]            = ImVec4(0.71f, 0.75f, 1.00f, 1.00f); // Lavender
		    colors[ImGuiCol_SliderGrab]           = ImVec4(0.45f, 0.78f, 0.93f, 1.00f); // Sapphire
		    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.45f, 0.78f, 0.93f, 1.00f);
		    colors[ImGuiCol_Button]               = ImVec4(0.19f, 0.20f, 0.27f, 1.00f);
		    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.80f, 0.65f, 0.97f, 1.00f); // Mauve
		    colors[ImGuiCol_ButtonActive]         = ImVec4(0.70f, 0.55f, 0.87f, 1.00f);
		    colors[ImGuiCol_Header]               = ImVec4(0.19f, 0.20f, 0.27f, 1.00f);
		    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.25f, 0.26f, 0.35f, 1.00f);
		    colors[ImGuiCol_HeaderActive]         = ImVec4(0.31f, 0.32f, 0.42f, 1.00f);
		    colors[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
		    colors[ImGuiCol_TabHovered]           = ImVec4(0.31f, 0.32f, 0.42f, 1.00f);
		    colors[ImGuiCol_TabSelected]          = ImVec4(0.19f, 0.20f, 0.27f, 1.00f);
		    colors[ImGuiCol_TabDimmed]            = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
		    colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
		    colors[ImGuiCol_PlotLines]            = ImVec4(0.94f, 0.72f, 0.42f, 1.00f); // Marigold
		    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.31f, 0.32f, 0.42f, 1.00f);
		    colors[ImGuiCol_NavCursor]            = ImVec4(0.71f, 0.75f, 1.00f, 1.00f); // Lavender
		    colors[ImGuiCol_DockingPreview]       = ImVec4(0.71f, 0.75f, 1.00f, 0.50f);
		    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
		}

		inline void SetupImGuiGruvboxHardStyle()
		{
		    ImGuiStyle& style = ImGui::GetStyle();
		    ImVec4* colors = style.Colors;

		    style.WindowPadding     = ImVec2(10.0f, 10.0f);
		    style.FramePadding      = ImVec2(6.0f, 4.0f);
		    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
		    style.ScrollbarSize     = 14.0f;
		    style.GrabMinSize       = 12.0f;
		    style.WindowRounding    = 2.0f;
		    style.FrameRounding     = 2.0f;
		    style.PopupRounding     = 2.0f;
		    style.ScrollbarRounding = 2.0f;
		    style.GrabRounding      = 2.0f;
		    style.TabRounding       = 2.0f;
		    style.WindowBorderSize  = 1.0f;
		    style.FrameBorderSize   = 1.0f;
		    style.PopupBorderSize   = 1.0f;

		    colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.86f, 0.70f, 1.00f); // #ebdbb2
		    colors[ImGuiCol_TextDisabled]         = ImVec4(0.57f, 0.51f, 0.45f, 1.00f); // #928374
		    colors[ImGuiCol_WindowBg]             = ImVec4(0.11f, 0.13f, 0.13f, 1.00f); // #1d2021
		    colors[ImGuiCol_ChildBg]              = ImVec4(0.11f, 0.13f, 0.13f, 0.00f);
		    colors[ImGuiCol_PopupBg]              = ImVec4(0.11f, 0.13f, 0.13f, 0.95f);
		    colors[ImGuiCol_Border]               = ImVec4(0.31f, 0.29f, 0.27f, 1.00f); // #504945
		    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		    colors[ImGuiCol_FrameBg]              = ImVec4(0.24f, 0.22f, 0.21f, 1.00f); // #3c3836
		    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.31f, 0.29f, 0.27f, 1.00f); // #504945
		    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.40f, 0.36f, 0.33f, 1.00f); // #665c54
		    colors[ImGuiCol_TitleBg]              = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
		    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
		    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
		    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.15f, 0.14f, 0.13f, 1.00f); // #282828
		    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.36f, 0.33f, 1.00f);
		    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.57f, 0.51f, 0.45f, 1.00f);
		    colors[ImGuiCol_CheckMark]            = ImVec4(0.72f, 0.73f, 0.15f, 1.00f); // #b8bb26 (Green)
		    colors[ImGuiCol_SliderGrab]           = ImVec4(0.51f, 0.65f, 0.60f, 1.00f); // #83a598 (Blue)
		    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.55f, 0.73f, 0.67f, 1.00f);
		    colors[ImGuiCol_Button]               = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
		    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.98f, 0.29f, 0.20f, 1.00f); // #fb4934 (Red)
		    colors[ImGuiCol_ButtonActive]         = ImVec4(0.80f, 0.20f, 0.15f, 1.00f);
		    colors[ImGuiCol_Header]               = ImVec4(0.24f, 0.22f, 0.21f, 1.00f);
		    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
		    colors[ImGuiCol_HeaderActive]         = ImVec4(0.40f, 0.36f, 0.33f, 1.00f);
		    colors[ImGuiCol_Tab]                  = ImVec4(0.24f, 0.22f, 0.21f, 1.00f);
		    colors[ImGuiCol_TabHovered]           = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
		    colors[ImGuiCol_TabSelected]          = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
		    colors[ImGuiCol_PlotLines]            = ImVec4(0.98f, 0.74f, 0.18f, 1.00f); // #fabd2f (Yellow)
		    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
		    colors[ImGuiCol_NavCursor]            = ImVec4(0.98f, 0.29f, 0.20f, 1.00f);
		    colors[ImGuiCol_DockingPreview]       = ImVec4(0.72f, 0.73f, 0.15f, 0.50f);
		    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
		}

		inline constexpr std::array kThemes
		{
			ThemeEntry{ "Dracula",          &SetupImGuiDraculaStyle         },
			ThemeEntry{ "Catppuccin Mocha", &SetupImGuiCatppuccinMochaStyle },
			ThemeEntry{ "Gruvbox Hard",     &SetupImGuiGruvboxHardStyle     },
		};
		static_assert(kThemes.size() == static_cast<size_t>(Theme::MAX_COUNT), "Theme enum and array size mismatch!");

		// Sizes are authored at 96 DPI, so sizeScale is the window's content scale
		inline void ApplyTheme(Theme theme, f32 sizeScale = 1.0f)
		{
			ImGuiStyle& style       = ImGui::GetStyle();
			const f32 fontSizeBase  = style.FontSizeBase;
			const f32 fontScaleMain = style.FontScaleMain;
			const f32 fontScaleDpi  = style.FontScaleDpi;

			style = ImGuiStyle{};
			style.FontSizeBase  = fontSizeBase;
			style.FontScaleMain = fontScaleMain;
			style.FontScaleDpi  = fontScaleDpi;

			kThemes[static_cast<size_t>(theme)].Apply();
			style.ScaleAllSizes(sizeScale);
		}
	}
}
