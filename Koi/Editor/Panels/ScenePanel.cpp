#include "Koi/Editor/Panels/ScenePanel.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Scene/Scene.h"

namespace koi
{
	void ScenePanel::OnDraw()
	{
		// TODO: Remove logic from the scene and bring it here
		if (Scene* scene = m_game->GetEditorScene())
			scene->DrawImGui();
	}
}
