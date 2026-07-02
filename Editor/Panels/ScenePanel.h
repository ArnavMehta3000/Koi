#pragma once
#include "Editor/EditorPanel.h"

namespace koi
{
	class AppBase;

	class ScenePanel : public IEditorPanel
	{
	public:
		explicit ScenePanel(AppBase& game) : m_game(&game) {}

		[[nodiscard]] std::string_view GetName() const override { return "Scene"; }
		void OnDraw() override;

	private:
		AppBase* m_game;
	};
}
