#pragma once
#include "Koi/Engine/ECS/World.h"

namespace koi::editor
{
	struct Selection
	{
		Entity Selected;

		[[nodiscard]] bool IsValid() const { return Selected.is_alive(); }
		void Clear() { Selected = Entity{}; }
	};
}
