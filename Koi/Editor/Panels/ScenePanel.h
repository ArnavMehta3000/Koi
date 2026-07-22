#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/ECS/World.h"

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
		AppBase* m_game = nullptr;
		Entity   m_selected;

		// Euler angles are UI state: re-deriving them from the quaternion every frame makes
		// the drag widget snap near gimbal lock, so cache them until the selection changes
		Vector3  m_eulerDeg = Vector3::Zero;
		EntityID m_eulerFor = 0;
	};
}
