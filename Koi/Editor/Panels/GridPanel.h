#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include <IconsFontAwesome6.h>

namespace koi
{
	class Renderer;
	class GridPass;
}

namespace koi::editor
{
	// Settings for the ground grid. Edits the pass's own Settings, which is the single
	// source of truth, so the Gizmos menu toggle and this panel cannot disagree
	class GridPanel : public IEditorPanel
	{
	public:
		explicit GridPanel(Renderer& renderer) : m_renderer(&renderer) {}

		[[nodiscard]] std::string_view GetName() const override { return "Grid"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_BORDER_ALL; }
		void OnDraw() override;

	private:
		[[nodiscard]] GridPass* FindPass() const;

		Renderer* m_renderer = nullptr;
	};
}
