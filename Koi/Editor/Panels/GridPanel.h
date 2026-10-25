#pragma once
#include "Koi/Editor/Core/Panel.h"
#include <IconsFontAwesome6.h>

namespace koi
{
	class Renderer;
	class GridPass;
}

namespace koi::editor
{
	class EditorContext;
	class UndoStack;

	// Settings for the ground grid. Edits the pass's own Settings, which is the single
	// source of truth, so the Gizmos menu toggle and this panel cannot disagree
	class GridPanel : public Panel
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "Grid", .Icon = ICON_FA_BORDER_ALL };

		explicit GridPanel(EditorContext* ctx);

		void OnDraw() override;

		void SaveState(toml::table& state) const override;
		void LoadState(const toml::table& state) override;

	private:
		[[nodiscard]] GridPass* FindPass() const;

		Renderer*  m_renderer = nullptr;
		UndoStack* m_undo     = nullptr;
	};
}
