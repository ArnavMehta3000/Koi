#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Editor/Selection.h"

namespace koi
{
	class AppBase;

	class ScenePanel : public IEditorPanel
	{
	public:
		ScenePanel(AppBase& game, EditorSelection& selection) : m_game(&game), m_selection(&selection) {}

		[[nodiscard]] std::string_view GetName() const override { return "Scene"; }
		void OnDraw() override;

	private:
		AppBase*         m_game      = nullptr;
		EditorSelection* m_selection = nullptr;
	};
}
