#include "Panels/ScenePanel.h"
#include "Runtime/AppBase.h"
#include "Scene/Scene.h"

namespace koi
{
	void ScenePanel::OnDraw()
	{
		// TODO: Remove logic from the scene and bring it here
		if (Scene* scene = m_game->GetEditorScene())
			scene->DrawImGui();
	}
}
