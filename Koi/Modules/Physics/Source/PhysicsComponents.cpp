#include "Koi/Modules/Physics/Source/PhysicsComponents.h"
#include "Koi/Engine/ECS/World.h"

namespace koi
{
	const PhysicsSettings& PhysicsSettings::Resolve(const World& world)
	{
		// Return first found or default (if there is no entity with physics setting component on it)

		const PhysicsSettings* found = nullptr;
		world.GetECS().Each<const PhysicsSettings>([&](const PhysicsSettings& settings)
		{
			if (!found) found = &settings;
		});
		static const PhysicsSettings kDefault{};
		return found ? *found : kDefault;
	}
}
