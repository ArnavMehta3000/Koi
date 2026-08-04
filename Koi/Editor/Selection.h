#pragma once
#include "Koi/Engine/ECS/World.h"

namespace koi
{
	struct EditorSelection
	{
		Entity Selected;

		[[nodiscard]] bool IsValid() const { return Selected.is_alive(); }
		void Clear() { Selected = Entity{}; }
	};
}
