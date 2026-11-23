#pragma once
#include "Koi/Editor/Panels/RenderViewport.h"
#include "Koi/Engine/ECS/PreviewScene.h"
#include <IconsFontAwesome6.h>
#include <memory>
#include <string>
#include <string_view>

namespace koi
{
	class SceneRenderer;
}

namespace koi::editor
{
	class EditorContext;
	class Selection;

	class AssetViewerPanel final : public RenderViewport
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "Asset Viewer", .Icon = ICON_FA_CUBE, .Padding = false };

		explicit AssetViewerPanel(EditorContext* ctx);
		~AssetViewerPanel() override;

		void OnTick(f32 dt) override;
		void OnRender() override;

		// Opened fresh each time, so there's no camera worth keeping
		void SaveState(toml::table&) const override {}
		void LoadState(const toml::table&) override {}

	private:
		void FollowSelection();
		void OnWindowGui() override;

	private:
		Selection*                     m_selection = nullptr;
		PreviewScene                   m_preview;
		std::unique_ptr<SceneRenderer> m_sceneRenderer;  // created on the first frame it's open
		std::string                    m_shown;
		std::string_view               m_unsupported;  // the selected asset's type name when it has no preview
		bool                           m_framePending = false;
	};
}
