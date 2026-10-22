#pragma once
#include "Koi/Editor/Core/EditorPanel.h"
#include <IconsFontAwesome6.h>

namespace koi { class Renderer; }

namespace koi::editor
{
	// Every texture version the render graph wrote last frame, captured and visualized one at a time
	class RenderTargetsPanel : public IEditorPanel
	{
	public:
		explicit RenderTargetsPanel(Renderer& renderer) : m_renderer(&renderer) {}

		[[nodiscard]] std::string_view GetName() const override { return "Render Targets"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_LAYER_GROUP; }
		void OnDraw() override;

	private:
		void DrawTargetList();
		void DrawPreview();

	private:
		Renderer* m_renderer = nullptr;
	};
}
