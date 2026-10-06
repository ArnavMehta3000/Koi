#pragma once
#include "Koi/Engine/Core/Types.h"
#include <imgui.h>
#include <KoiProfiler.h>
#include <array>

namespace koi::editor
{
	enum class Theme : u8
	{
		Zero,
		Obsidian,
		MAX_COUNT
	};

	// A theme only changes colors; sizes and rounding are shared by all of them
	struct ThemeEntry
	{
		const char*             Name;
		void                  (*ApplyColors)(ImVec4* colors);
		prof::GoldenRatioParams ProfilerPalette;
	};

	namespace utils
	{
		static_assert(ImGuiCol_COUNT == 63, "ImGuiCol set changed, update the editor themes");

		inline constexpr ImVec4 HexColor(u32 rgb, f32 alpha = 1.0f)
		{
			return ImVec4(static_cast<f32>((rgb >> 16) & 0xFF) / 255.0f,
						  static_cast<f32>((rgb >> 8) & 0xFF) / 255.0f,
						  static_cast<f32>(rgb & 0xFF) / 255.0f,
						  alpha);
		}

		inline void ApplyEditorStyle(ImGuiStyle& style)
		{
			style.WindowPadding            = ImVec2(8.0f, 8.0f);
			style.FramePadding             = ImVec2(6.0f, 4.0f);
			style.CellPadding              = ImVec2(6.0f, 3.0f);
			style.ItemSpacing              = ImVec2(8.0f, 4.0f);
			style.ItemInnerSpacing         = ImVec2(4.0f, 4.0f);
			style.SeparatorTextPadding     = ImVec2(12.0f, 3.0f);
			style.IndentSpacing            = 16.0f;
			style.ScrollbarSize            = 12.0f;
			style.ScrollbarPadding         = 2.0f;
			style.GrabMinSize              = 10.0f;
			style.WindowRounding           = 0.0f;
			style.ChildRounding            = 3.0f;
			style.FrameRounding            = 3.0f;
			style.PopupRounding            = 4.0f;
			style.ScrollbarRounding        = 6.0f;
			style.GrabRounding             = 2.0f;
			style.TabRounding              = 3.0f;
			style.MenuItemRounding         = 3.0f;
			style.DragDropTargetRounding   = 3.0f;
			style.WindowBorderSize         = 1.0f;
			style.ChildBorderSize          = 1.0f;
			style.PopupBorderSize          = 1.0f;
			style.FrameBorderSize          = 1.0f;
			style.TabBorderSize            = 0.0f;
			style.TabBarBorderSize         = 1.0f;
			style.TabBarOverlineSize       = 2.0f;
			style.SeparatorTextBorderSize  = 1.0f;
			style.DockingSeparatorSize     = 2.0f;
			style.WindowMenuButtonPosition = ImGuiDir_Right;
		}

		// Port of the Zero Visual Studio 2026 theme (AgitoReiKen)
		inline void ApplyZeroColors(ImVec4* colors)
		{
			constexpr ImVec4 kEnvironment = HexColor(0x0F1116); // EnvironmentBackground, ToolWindowBackground
			constexpr ImVec4 kSurface     = HexColor(0x101319); // editor, PanelTitleBar, CommandBar
			constexpr ImVec4 kBorder      = HexColor(0x15181F); // ActiveBorder, AccentBorder
			constexpr ImVec4 kRaised      = HexColor(0x171A21); // ComboBoxPopupBorder, SolidBackgroundFillQuaternary
			constexpr ImVec4 kRaisedHover = HexColor(0x1C2027); // TextBoxBorderFocused (VS2022 Zero)
			constexpr ImVec4 kSelection   = HexColor(0x2B3959); // Selected Text, ScrollBarThumb
			constexpr ImVec4 kMuted       = HexColor(0x4B5B7E); // ToolWindowTabText, ButtonBorder
			constexpr ImVec4 kSecondary   = HexColor(0x8398B9); // TextFillSecondary, ButtonBorderFocused
			constexpr ImVec4 kText        = HexColor(0xC2CCE0); // ToolWindowText
			constexpr ImVec4 kAccent      = HexColor(0x2A7195); // AccentFillDefault
			constexpr ImVec4 kAccentHi    = HexColor(0x2AA1D9); // FileTabLastActiveDocumentBorder
			constexpr ImVec4 kLink        = HexColor(0x60A5FA); // PanelHyperlink
			constexpr ImVec4 kNone        = HexColor(0x000000, 0.0f);

			colors[ImGuiCol_Text]                      = kText;
			colors[ImGuiCol_TextDisabled]              = kMuted;
			colors[ImGuiCol_WindowBg]                  = kEnvironment;
			colors[ImGuiCol_ChildBg]                   = kNone;
			colors[ImGuiCol_PopupBg]                   = kEnvironment;
			colors[ImGuiCol_Border]                    = kBorder;
			colors[ImGuiCol_BorderShadow]              = kNone;
			colors[ImGuiCol_FrameBg]                   = kRaised;
			colors[ImGuiCol_FrameBgHovered]            = kRaisedHover;
			colors[ImGuiCol_FrameBgActive]             = kSelection;
			colors[ImGuiCol_TitleBg]                   = kEnvironment;
			colors[ImGuiCol_TitleBgActive]             = kSurface;
			colors[ImGuiCol_TitleBgCollapsed]          = kEnvironment;
			colors[ImGuiCol_MenuBarBg]                 = kSurface;
			colors[ImGuiCol_ScrollbarBg]               = kNone;
			colors[ImGuiCol_ScrollbarGrab]             = HexColor(0x2B3959, 0.55f);
			colors[ImGuiCol_ScrollbarGrabHovered]      = HexColor(0x2B3959, 0.85f);
			colors[ImGuiCol_ScrollbarGrabActive]       = kMuted;
			colors[ImGuiCol_CheckMark]                 = kText;
			colors[ImGuiCol_CheckboxSelectedBg]        = kAccent;
			colors[ImGuiCol_SliderGrab]                = kMuted;
			colors[ImGuiCol_SliderGrabActive]          = kAccentHi;
			colors[ImGuiCol_Button]                    = kRaised;
			colors[ImGuiCol_ButtonHovered]             = kRaisedHover;
			colors[ImGuiCol_ButtonActive]              = kAccent;
			colors[ImGuiCol_Header]                    = kRaised;
			colors[ImGuiCol_HeaderHovered]             = kRaisedHover;
			colors[ImGuiCol_HeaderActive]              = kSelection;
			colors[ImGuiCol_Separator]                 = kBorder;
			colors[ImGuiCol_SeparatorHovered]          = kAccent;
			colors[ImGuiCol_SeparatorActive]           = kAccentHi;
			colors[ImGuiCol_ResizeGrip]                = HexColor(0x2B3959, 0.20f);
			colors[ImGuiCol_ResizeGripHovered]         = kAccent;
			colors[ImGuiCol_ResizeGripActive]          = kAccentHi;
			colors[ImGuiCol_InputTextCursor]           = kText;
			colors[ImGuiCol_TabHovered]                = HexColor(0x808080, 0.20f); // ToolWindowTabMouseOverBackground
			colors[ImGuiCol_Tab]                       = kEnvironment;
			colors[ImGuiCol_TabSelected]               = kSurface;
			colors[ImGuiCol_TabSelectedOverline]       = kAccentHi;
			colors[ImGuiCol_TabDimmed]                 = kEnvironment;
			colors[ImGuiCol_TabDimmedSelected]         = kSurface;
			colors[ImGuiCol_TabDimmedSelectedOverline] = kNone;
			colors[ImGuiCol_DockingPreview]            = HexColor(0x2A7195, 0.70f);
			colors[ImGuiCol_DockingEmptyBg]            = kEnvironment;
			colors[ImGuiCol_PlotLines]                 = kSecondary;
			colors[ImGuiCol_PlotLinesHovered]          = kAccentHi;
			colors[ImGuiCol_PlotHistogram]             = kAccent;
			colors[ImGuiCol_PlotHistogramHovered]      = kAccentHi;
			colors[ImGuiCol_TableHeaderBg]             = kSurface;
			colors[ImGuiCol_TableBorderStrong]         = kRaised;
			colors[ImGuiCol_TableBorderLight]          = kBorder;
			colors[ImGuiCol_TableRowBg]                = kNone;
			colors[ImGuiCol_TableRowBgAlt]             = HexColor(0xFFFFFF, 0.02f);
			colors[ImGuiCol_TextLink]                  = kLink;
			colors[ImGuiCol_TextSelectedBg]            = kSelection;
			colors[ImGuiCol_TreeLines]                 = kRaised;
			colors[ImGuiCol_DragDropTarget]            = kAccentHi;
			colors[ImGuiCol_DragDropTargetBg]          = HexColor(0x062F4A, 0.60f); // TreeView DragOverItem
			colors[ImGuiCol_UnsavedMarker]             = kText;
			colors[ImGuiCol_NavCursor]                 = kSecondary;
			colors[ImGuiCol_NavWindowingHighlight]     = HexColor(0xC2CCE0, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg]         = HexColor(0x0F1116, 0.60f);
			colors[ImGuiCol_ModalWindowDimBg]          = HexColor(0x0F1116, 0.60f);
		}

		// Near-black neutrals with a single teal-blue accent
		inline void ApplyObsidianColors(ImVec4* colors)
		{
			constexpr ImVec4 kBase       = HexColor(0x09090B);
			constexpr ImVec4 kWindow     = HexColor(0x0E0E11);
			constexpr ImVec4 kElevated   = HexColor(0x141418);
			constexpr ImVec4 kControl    = HexColor(0x18181C);
			constexpr ImVec4 kControlHi  = HexColor(0x212126);
			constexpr ImVec4 kControlAct = HexColor(0x2A2A31);
			constexpr ImVec4 kBorder     = HexColor(0x232328);
			constexpr ImVec4 kBorderHi   = HexColor(0x313138);
			constexpr ImVec4 kText       = HexColor(0xEDEDEF);
			constexpr ImVec4 kSecondary  = HexColor(0xA1A1AA);
			constexpr ImVec4 kMuted      = HexColor(0x63636E);
			constexpr ImVec4 kAccent     = HexColor(0x26A3C2);
			constexpr ImVec4 kAccentHi   = HexColor(0x4CC0DC);
			constexpr ImVec4 kAccentLo   = HexColor(0x1B7891);
			constexpr ImVec4 kNone       = HexColor(0x000000, 0.0f);

			colors[ImGuiCol_Text]                      = kText;
			colors[ImGuiCol_TextDisabled]              = kMuted;
			colors[ImGuiCol_WindowBg]                  = kWindow;
			colors[ImGuiCol_ChildBg]                   = kNone;
			colors[ImGuiCol_PopupBg]                   = kElevated;
			colors[ImGuiCol_Border]                    = kBorder;
			colors[ImGuiCol_BorderShadow]              = kNone;
			colors[ImGuiCol_FrameBg]                   = kControl;
			colors[ImGuiCol_FrameBgHovered]            = kControlHi;
			colors[ImGuiCol_FrameBgActive]             = kControlAct;
			colors[ImGuiCol_TitleBg]                   = kBase;
			colors[ImGuiCol_TitleBgActive]             = kBase;
			colors[ImGuiCol_TitleBgCollapsed]          = kBase;
			colors[ImGuiCol_MenuBarBg]                 = kBase;
			colors[ImGuiCol_ScrollbarBg]               = kNone;
			colors[ImGuiCol_ScrollbarGrab]             = kBorderHi;
			colors[ImGuiCol_ScrollbarGrabHovered]      = HexColor(0x45454E);
			colors[ImGuiCol_ScrollbarGrabActive]       = kMuted;
			colors[ImGuiCol_CheckMark]                 = kAccent;
			colors[ImGuiCol_CheckboxSelectedBg]        = HexColor(0x26A3C2, 0.15f);
			colors[ImGuiCol_SliderGrab]                = kAccentLo;
			colors[ImGuiCol_SliderGrabActive]          = kAccentHi;
			colors[ImGuiCol_Button]                    = kControl;
			colors[ImGuiCol_ButtonHovered]             = kControlHi;
			colors[ImGuiCol_ButtonActive]              = kControlAct;
			colors[ImGuiCol_Header]                    = kControl;
			colors[ImGuiCol_HeaderHovered]             = kControlHi;
			colors[ImGuiCol_HeaderActive]              = kControlAct;
			colors[ImGuiCol_Separator]                 = kBorder;
			colors[ImGuiCol_SeparatorHovered]          = kAccentLo;
			colors[ImGuiCol_SeparatorActive]           = kAccent;
			colors[ImGuiCol_ResizeGrip]                = kNone;
			colors[ImGuiCol_ResizeGripHovered]         = HexColor(0x26A3C2, 0.60f);
			colors[ImGuiCol_ResizeGripActive]          = kAccent;
			colors[ImGuiCol_InputTextCursor]           = kAccent;
			colors[ImGuiCol_TabHovered]                = kControlHi;
			colors[ImGuiCol_Tab]                       = kBase;
			colors[ImGuiCol_TabSelected]               = kWindow;
			colors[ImGuiCol_TabSelectedOverline]       = kAccent;
			colors[ImGuiCol_TabDimmed]                 = kBase;
			colors[ImGuiCol_TabDimmedSelected]         = kWindow;
			colors[ImGuiCol_TabDimmedSelectedOverline] = kBorderHi;
			colors[ImGuiCol_DockingPreview]            = HexColor(0x26A3C2, 0.45f);
			colors[ImGuiCol_DockingEmptyBg]            = kBase;
			colors[ImGuiCol_PlotLines]                 = kSecondary;
			colors[ImGuiCol_PlotLinesHovered]          = kAccentHi;
			colors[ImGuiCol_PlotHistogram]             = kAccent;
			colors[ImGuiCol_PlotHistogramHovered]      = kAccentHi;
			colors[ImGuiCol_TableHeaderBg]             = kElevated;
			colors[ImGuiCol_TableBorderStrong]         = kBorderHi;
			colors[ImGuiCol_TableBorderLight]          = kBorder;
			colors[ImGuiCol_TableRowBg]                = kNone;
			colors[ImGuiCol_TableRowBgAlt]             = HexColor(0xFFFFFF, 0.025f);
			colors[ImGuiCol_TextLink]                  = kAccentHi;
			colors[ImGuiCol_TextSelectedBg]            = HexColor(0x26A3C2, 0.30f);
			colors[ImGuiCol_TreeLines]                 = kBorderHi;
			colors[ImGuiCol_DragDropTarget]            = kAccent;
			colors[ImGuiCol_DragDropTargetBg]          = HexColor(0x26A3C2, 0.10f);
			colors[ImGuiCol_UnsavedMarker]             = kAccent;
			colors[ImGuiCol_NavCursor]                 = kAccent;
			colors[ImGuiCol_NavWindowingHighlight]     = HexColor(0xEDEDEF, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg]         = HexColor(0x000000, 0.60f);
			colors[ImGuiCol_ModalWindowDimBg]          = HexColor(0x000000, 0.60f);
		}

		// Profiler task colors: Zero stays muted to match its low-contrast palette, Obsidian is crisper
		inline constexpr std::array kThemes
		{
			ThemeEntry{ "Zero",     &ApplyZeroColors,     { .HueStart = 0.55f, .Saturation = 0.45f, .SaturationAlt = 0.32f, .Value = 0.78f, .ValueAlt = 0.62f } },
			ThemeEntry{ "Obsidian", &ApplyObsidianColors, { .HueStart = 0.52f, .Saturation = 0.62f, .SaturationAlt = 0.45f, .Value = 0.90f, .ValueAlt = 0.74f } },
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

			const ThemeEntry& entry = kThemes[static_cast<size_t>(theme)];
			ApplyEditorStyle(style);
			entry.ApplyColors(style.Colors);
			style.ScaleAllSizes(sizeScale);

			static prof::GoldenRatioParams s_profilerPalette;
			s_profilerPalette = entry.ProfilerPalette;
			prof::ColorPalette& palette = prof::DefaultPalette();
			palette.GeneratorUserData   = &s_profilerPalette;
			palette.Regenerate();
		}
	}
}
