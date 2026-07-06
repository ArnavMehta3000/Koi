#pragma once
#include "Editor/EditorPanel.h"
#include "Editor/ViewportTarget.h"

namespace koi
{
	class Renderer;
	class CommandList;

	class ViewportPanel : public IEditorPanel
	{
	public:
		explicit ViewportPanel(Renderer& renderer);
		~ViewportPanel() override;

		[[nodiscard]] std::string_view GetName() const override { return "Viewport"; }
		void OnDraw() override;
		const ViewportTarget* PrepareFrame(CommandList& cmd);

		[[nodiscard]] const ViewportTarget* GetTarget() const { return m_valid ? &m_target : nullptr; }

	private:
		void EnsureSize(u32 width, u32 height);

	private:
		Renderer*      m_renderer;
		ViewportTarget m_target{};
		bool           m_valid         = false;
		u32            m_desiredWidth  = 1280;
		u32            m_desiredHeight = 720;
	};
}
