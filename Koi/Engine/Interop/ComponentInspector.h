#pragma once
#include "Koi/Engine/ECS/World.h"
#include <string_view>

namespace koi
{
	// Draws one component's editor UI
	class IComponentInspector
	{
	public:
		virtual ~IComponentInspector() = default;

		[[nodiscard]] virtual std::string_view Key() const = 0;
		virtual void Draw(Entity e) = 0;
	};
}
