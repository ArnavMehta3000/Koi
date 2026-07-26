#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/Core/Types.h"

namespace koi
{
	class Renderer;

	class GBufferDebugPanel : public IEditorPanel
	{
	public:
		explicit GBufferDebugPanel(Renderer& renderer) : m_renderer(&renderer) {}

		[[nodiscard]] std::string_view GetName() const override { return "GBuffer Debug"; }
		void OnDraw() override;

	private:
		Renderer* m_renderer = nullptr;
		i32       m_selected = 0;  // which output channel is shown
	};
}
