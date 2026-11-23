#pragma once
#include "Koi/Modules/Physics/Source/PhysicsModule.h"

namespace koi::editor { class EditorContext; }

namespace koi
{
	struct PhysicsModule::Editor
	{
		static void Register(editor::EditorContext& ctx);
	};
}
