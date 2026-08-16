#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include <string>

namespace koi { class Renderer; }

namespace koi::editor
{
	// Every output attachment of every pass in the frame graph, one at a time
	class RenderTargetsPanel : public IEditorPanel
	{
	public:
		explicit RenderTargetsPanel(Renderer& renderer) : m_renderer(&renderer) {}

		[[nodiscard]] std::string_view GetName() const override { return "Render Targets"; }
		void OnDraw() override;

	private:
		Renderer* m_renderer = nullptr;

		// Selection is kept by label, not index: passes create their targets lazily, so the
		// list can change size between frames
		std::string m_selected;
	};
}
