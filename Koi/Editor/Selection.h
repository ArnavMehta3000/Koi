#pragma once
#include "Koi/Engine/ECS/World.h"

namespace koi::editor
{
	struct Selection
	{
		Entity      Selected;
		std::string SelectedAsset;  // virtual path, from the Assets panel

		[[nodiscard]] bool IsValid() const { return Selected.is_alive(); }
		void Clear() { Selected = Entity{}; }
	};
}
