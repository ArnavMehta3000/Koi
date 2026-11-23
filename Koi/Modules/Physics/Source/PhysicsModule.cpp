#include "Koi/Modules/Physics/Source/PhysicsModule.h"
#include "Koi/Modules/Physics/Source/PhysicsComponents.h"
#include "Koi/Modules/Physics/Source/PhysicsDebugDraw.h"
#include "Koi/Modules/Physics/Source/PhysicsSystem.h"
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Core/ReflectSerializer.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"

namespace koi
{
	void PhysicsModule::Register()
	{
		ComponentRegistry& components = ComponentRegistry::Get();
		components.Register<ComponentType<PhysicsSettings>>(45);
		components.Register<ComponentType<RigidBody>>(46);
		components.Register<ComponentType<Collider>>(47);

		SystemRegistry& systems = SystemRegistry::Get();
		systems.Register<PhysicsSystem>();
#if KOI_DEBUG_DRAW
		systems.Register<PhysicsDebugDrawSystem>();
#endif
	}
}
