#pragma once
#include "Koi/Editor/ImGui/EditorPalette.h"
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
		EditorPalette           Palette;
		void                  (*ApplyColors)(const EditorPalette& palette, ImVec4* colors);
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

		inline constexpr ImVec4 WithAlpha(ImVec4 color, f32 alpha)
		{
			color.w = alpha;
			return color;
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
		inline constexpr EditorPalette kZeroPalette
		{
			.Base          = HexColor(0x0F1116), // EnvironmentBackground, ToolWindowBackground
			.Surface       = HexColor(0x101319), // editor, PanelTitleBar, CommandBar
			.Elevated      = HexColor(0x101319), // GridHeadingBackground
			.Raised        = HexColor(0x171A21), // ComboBoxPopupBorder, SolidBackgroundFillQuaternary
			.RaisedHover   = HexColor(0x1C2027), // TextBoxBorderFocused (VS2022 Zero)
			.RaisedActive  = HexColor(0x2B3959), // Selected Text
			.Border        = HexColor(0x15181F), // ActiveBorder, AccentBorder
			.BorderStrong  = HexColor(0x171A21), // ComboBoxPopupBorder
			.Selection     = HexColor(0x2B3959), // Selected Text, ScrollBarThumb
			.Text          = HexColor(0xC2CCE0), // ToolWindowText
			.TextSecondary = HexColor(0x8398B9), // TextFillSecondary, ButtonBorderFocused
			.TextDisabled  = HexColor(0x4B5B7E), // ToolWindowTabText, ButtonBorder
			.Accent        = HexColor(0x2A7195), // AccentFillDefault
			.AccentHover   = HexColor(0x2AA1D9), // FileTabLastActiveDocumentBorder
			.AccentMuted   = HexColor(0x1F5470),
			.Link          = HexColor(0x60A5FA), // PanelHyperlink
			.Info          = HexColor(0x5EABDF), // keywords
			.Success       = HexColor(0x9AA084), // strings
			.Warning       = HexColor(0xB4911B), // ProgressBar IndicatorFillWarning
			.Error         = HexColor(0xDA736C), // ProgressBar IndicatorFillCritical
			.Playing       = HexColor(0xC99A45),
			.Paused        = HexColor(0x8C7444),
		};

		inline void ApplyZeroColors(const EditorPalette& p, ImVec4* colors)
		{
			constexpr ImVec4 kNone = HexColor(0x000000, 0.0f);

			colors[ImGuiCol_Text]                      = p.Text;
			colors[ImGuiCol_TextDisabled]              = p.TextDisabled;
			colors[ImGuiCol_WindowBg]                  = p.Base;
			colors[ImGuiCol_ChildBg]                   = kNone;
			colors[ImGuiCol_PopupBg]                   = p.Base;
			colors[ImGuiCol_Border]                    = p.Border;
			colors[ImGuiCol_BorderShadow]              = kNone;
			colors[ImGuiCol_FrameBg]                   = p.Raised;
			colors[ImGuiCol_FrameBgHovered]            = p.RaisedHover;
			colors[ImGuiCol_FrameBgActive]             = p.RaisedActive;
			colors[ImGuiCol_TitleBg]                   = p.Base;
			colors[ImGuiCol_TitleBgActive]             = p.Surface;
			colors[ImGuiCol_TitleBgCollapsed]          = p.Base;
			colors[ImGuiCol_MenuBarBg]                 = p.Surface;
			colors[ImGuiCol_ScrollbarBg]               = kNone;
			colors[ImGuiCol_ScrollbarGrab]             = WithAlpha(p.Selection, 0.55f);
			colors[ImGuiCol_ScrollbarGrabHovered]      = WithAlpha(p.Selection, 0.85f);
			colors[ImGuiCol_ScrollbarGrabActive]       = p.TextDisabled;
			colors[ImGuiCol_CheckMark]                 = p.Text;
			colors[ImGuiCol_CheckboxSelectedBg]        = p.Accent;
			colors[ImGuiCol_SliderGrab]                = p.TextDisabled;
			colors[ImGuiCol_SliderGrabActive]          = p.AccentHover;
			colors[ImGuiCol_Button]                    = p.Raised;
			colors[ImGuiCol_ButtonHovered]             = p.RaisedHover;
			colors[ImGuiCol_ButtonActive]              = p.Accent;
			colors[ImGuiCol_Header]                    = p.Raised;
			colors[ImGuiCol_HeaderHovered]             = p.RaisedHover;
			colors[ImGuiCol_HeaderActive]              = p.RaisedActive;
			colors[ImGuiCol_Separator]                 = p.Border;
			colors[ImGuiCol_SeparatorHovered]          = p.Accent;
			colors[ImGuiCol_SeparatorActive]           = p.AccentHover;
			colors[ImGuiCol_ResizeGrip]                = WithAlpha(p.Selection, 0.20f);
			colors[ImGuiCol_ResizeGripHovered]         = p.Accent;
			colors[ImGuiCol_ResizeGripActive]          = p.AccentHover;
			colors[ImGuiCol_InputTextCursor]           = p.Text;
			colors[ImGuiCol_TabHovered]                = HexColor(0x808080, 0.20f); // ToolWindowTabMouseOverBackground
			colors[ImGuiCol_Tab]                       = p.Base;
			colors[ImGuiCol_TabSelected]               = p.Surface;
			colors[ImGuiCol_TabSelectedOverline]       = p.AccentHover;
			colors[ImGuiCol_TabDimmed]                 = p.Base;
			colors[ImGuiCol_TabDimmedSelected]         = p.Surface;
			colors[ImGuiCol_TabDimmedSelectedOverline] = kNone;
			colors[ImGuiCol_DockingPreview]            = WithAlpha(p.Accent, 0.70f);
			colors[ImGuiCol_DockingEmptyBg]            = p.Base;
			colors[ImGuiCol_PlotLines]                 = p.TextSecondary;
			colors[ImGuiCol_PlotLinesHovered]          = p.AccentHover;
			colors[ImGuiCol_PlotHistogram]             = p.Accent;
			colors[ImGuiCol_PlotHistogramHovered]      = p.AccentHover;
			colors[ImGuiCol_TableHeaderBg]             = p.Elevated;
			colors[ImGuiCol_TableBorderStrong]         = p.BorderStrong;
			colors[ImGuiCol_TableBorderLight]          = p.Border;
			colors[ImGuiCol_TableRowBg]                = kNone;
			colors[ImGuiCol_TableRowBgAlt]             = HexColor(0xFFFFFF, 0.02f);
			colors[ImGuiCol_TextLink]                  = p.Link;
			colors[ImGuiCol_TextSelectedBg]            = p.Selection;
			colors[ImGuiCol_TreeLines]                 = p.Raised;
			colors[ImGuiCol_DragDropTarget]            = p.AccentHover;
			colors[ImGuiCol_DragDropTargetBg]          = HexColor(0x062F4A, 0.60f); // TreeView DragOverItem
			colors[ImGuiCol_UnsavedMarker]             = p.Text;
			colors[ImGuiCol_NavCursor]                 = p.TextSecondary;
			colors[ImGuiCol_NavWindowingHighlight]     = WithAlpha(p.Text, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg]         = WithAlpha(p.Base, 0.60f);
			colors[ImGuiCol_ModalWindowDimBg]          = WithAlpha(p.Base, 0.60f);
		}

		// Near-black neutrals with a single teal-blue accent
		inline constexpr EditorPalette kObsidianPalette
		{
			.Base          = HexColor(0x09090B),
			.Surface       = HexColor(0x0E0E11),
			.Elevated      = HexColor(0x141418),
			.Raised        = HexColor(0x18181C),
			.RaisedHover   = HexColor(0x212126),
			.RaisedActive  = HexColor(0x2A2A31),
			.Border        = HexColor(0x232328),
			.BorderStrong  = HexColor(0x313138),
			.Selection     = HexColor(0x26A3C2, 0.30f),
			.Text          = HexColor(0xEDEDEF),
			.TextSecondary = HexColor(0xA1A1AA),
			.TextDisabled  = HexColor(0x63636E),
			.Accent        = HexColor(0x26A3C2),
			.AccentHover   = HexColor(0x4CC0DC),
			.AccentMuted   = HexColor(0x1B7891),
			.Link          = HexColor(0x4CC0DC),
			.Info          = HexColor(0x5BA4F5),
			.Success       = HexColor(0x5BC88A),
			.Warning       = HexColor(0xE8B04A),
			.Error         = HexColor(0xEF6461),
			.Playing       = HexColor(0xF5A524),
			.Paused        = HexColor(0xB07E2E),
		};

		inline void ApplyObsidianColors(const EditorPalette& p, ImVec4* colors)
		{
			constexpr ImVec4 kNone = HexColor(0x000000, 0.0f);

			colors[ImGuiCol_Text]                      = p.Text;
			colors[ImGuiCol_TextDisabled]              = p.TextDisabled;
			colors[ImGuiCol_WindowBg]                  = p.Surface;
			colors[ImGuiCol_ChildBg]                   = kNone;
			colors[ImGuiCol_PopupBg]                   = p.Elevated;
			colors[ImGuiCol_Border]                    = p.Border;
			colors[ImGuiCol_BorderShadow]              = kNone;
			colors[ImGuiCol_FrameBg]                   = p.Raised;
			colors[ImGuiCol_FrameBgHovered]            = p.RaisedHover;
			colors[ImGuiCol_FrameBgActive]             = p.RaisedActive;
			colors[ImGuiCol_TitleBg]                   = p.Base;
			colors[ImGuiCol_TitleBgActive]             = p.Base;
			colors[ImGuiCol_TitleBgCollapsed]          = p.Base;
			colors[ImGuiCol_MenuBarBg]                 = p.Base;
			colors[ImGuiCol_ScrollbarBg]               = kNone;
			colors[ImGuiCol_ScrollbarGrab]             = p.BorderStrong;
			colors[ImGuiCol_ScrollbarGrabHovered]      = HexColor(0x45454E);
			colors[ImGuiCol_ScrollbarGrabActive]       = p.TextDisabled;
			colors[ImGuiCol_CheckMark]                 = p.Accent;
			colors[ImGuiCol_CheckboxSelectedBg]        = WithAlpha(p.Accent, 0.15f);
			colors[ImGuiCol_SliderGrab]                = p.AccentMuted;
			colors[ImGuiCol_SliderGrabActive]          = p.AccentHover;
			colors[ImGuiCol_Button]                    = p.Raised;
			colors[ImGuiCol_ButtonHovered]             = p.RaisedHover;
			colors[ImGuiCol_ButtonActive]              = p.RaisedActive;
			colors[ImGuiCol_Header]                    = p.Raised;
			colors[ImGuiCol_HeaderHovered]             = p.RaisedHover;
			colors[ImGuiCol_HeaderActive]              = p.RaisedActive;
			colors[ImGuiCol_Separator]                 = p.Border;
			colors[ImGuiCol_SeparatorHovered]          = p.AccentMuted;
			colors[ImGuiCol_SeparatorActive]           = p.Accent;
			colors[ImGuiCol_ResizeGrip]                = kNone;
			colors[ImGuiCol_ResizeGripHovered]         = WithAlpha(p.Accent, 0.60f);
			colors[ImGuiCol_ResizeGripActive]          = p.Accent;
			colors[ImGuiCol_InputTextCursor]           = p.Accent;
			colors[ImGuiCol_TabHovered]                = p.RaisedHover;
			colors[ImGuiCol_Tab]                       = p.Base;
			colors[ImGuiCol_TabSelected]               = p.Surface;
			colors[ImGuiCol_TabSelectedOverline]       = p.Accent;
			colors[ImGuiCol_TabDimmed]                 = p.Base;
			colors[ImGuiCol_TabDimmedSelected]         = p.Surface;
			colors[ImGuiCol_TabDimmedSelectedOverline] = p.BorderStrong;
			colors[ImGuiCol_DockingPreview]            = WithAlpha(p.Accent, 0.45f);
			colors[ImGuiCol_DockingEmptyBg]            = p.Base;
			colors[ImGuiCol_PlotLines]                 = p.TextSecondary;
			colors[ImGuiCol_PlotLinesHovered]          = p.AccentHover;
			colors[ImGuiCol_PlotHistogram]             = p.Accent;
			colors[ImGuiCol_PlotHistogramHovered]      = p.AccentHover;
			colors[ImGuiCol_TableHeaderBg]             = p.Elevated;
			colors[ImGuiCol_TableBorderStrong]         = p.BorderStrong;
			colors[ImGuiCol_TableBorderLight]          = p.Border;
			colors[ImGuiCol_TableRowBg]                = kNone;
			colors[ImGuiCol_TableRowBgAlt]             = HexColor(0xFFFFFF, 0.025f);
			colors[ImGuiCol_TextLink]                  = p.Link;
			colors[ImGuiCol_TextSelectedBg]            = p.Selection;
			colors[ImGuiCol_TreeLines]                 = p.BorderStrong;
			colors[ImGuiCol_DragDropTarget]            = p.Accent;
			colors[ImGuiCol_DragDropTargetBg]          = WithAlpha(p.Accent, 0.10f);
			colors[ImGuiCol_UnsavedMarker]             = p.Accent;
			colors[ImGuiCol_NavCursor]                 = p.Accent;
			colors[ImGuiCol_NavWindowingHighlight]     = WithAlpha(p.Text, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg]         = HexColor(0x000000, 0.60f);
			colors[ImGuiCol_ModalWindowDimBg]          = HexColor(0x000000, 0.60f);
		}

		// Profiler task colors: Zero stays muted to match its low-contrast palette, Obsidian is crisper
		inline constexpr std::array kThemes
		{
			ThemeEntry{ "Zero",     kZeroPalette,     &ApplyZeroColors,     { .HueStart = 0.55f, .Saturation = 0.45f, .SaturationAlt = 0.32f, .Value = 0.78f, .ValueAlt = 0.62f } },
			ThemeEntry{ "Obsidian", kObsidianPalette, &ApplyObsidianColors, { .HueStart = 0.52f, .Saturation = 0.62f, .SaturationAlt = 0.45f, .Value = 0.90f, .ValueAlt = 0.74f } },
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
			EditorPalette::SetActive(entry.Palette);
			ApplyEditorStyle(style);
			entry.ApplyColors(entry.Palette, style.Colors);
			style.ScaleAllSizes(sizeScale);

			static prof::GoldenRatioParams s_profilerPalette;
			s_profilerPalette = entry.ProfilerPalette;
			prof::ColorPalette& palette = prof::DefaultPalette();
			palette.GeneratorUserData   = &s_profilerPalette;
			palette.Regenerate();
		}
	}
}
