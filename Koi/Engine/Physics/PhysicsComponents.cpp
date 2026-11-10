#include "Koi/Engine/Physics/PhysicsComponents.h"
#include "Koi/Engine/ECS/World.h"

namespace koi
{
	const PhysicsSettings& PhysicsSettings::Resolve(const World& world)
	{
		const PhysicsSettings* found = nullptr;
		world.GetECS().query<const PhysicsSettings>().each([&](const PhysicsSettings& settings)
		{
			if (!found) found = &settings;
		});
		static const PhysicsSettings kDefault{};
		return found ? *found : kDefault;
	}
}
