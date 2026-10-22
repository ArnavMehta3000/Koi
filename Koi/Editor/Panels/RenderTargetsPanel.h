#pragma once
#include "Koi/Editor/Core/Panel.h"
#include <IconsFontAwesome6.h>

namespace koi { class Renderer; }

namespace koi::editor
{
	// Every texture version the render graph wrote last frame, captured and visualized one at a time
	class RenderTargetsPanel : public Panel
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "Render Targets", .Icon = ICON_FA_LAYER_GROUP };

		RenderTargetsPanel();

		void OnDraw() override;

	private:
		void DrawTargetList();
		void DrawPreview();

	private:
		Renderer* m_renderer = nullptr;
	};
}
