#include "SurfApp.h"
#include "Components/Obstacle.h"
#include "Components/PlayerMover.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Interop/ReflectInspector.h"

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication([[maybe_unused]] CommandLine& cmd)
	{
		return std::make_unique<game::SurfApp>();
	}
}

namespace game
{
	void SurfApp::OnRegisterComponents()
	{
		koi::ComponentRegistry& registry = koi::ComponentRegistry::Get();
		registry.Register<koi::ComponentType<PlayerMover>>(50);
		registry.Register<koi::ComponentType<Obstacle>>(51);
		registry.Register<koi::ComponentType<ObstacleSpawner>>(52);
	}

	void SurfApp::OnRegisterSystems()
	{
		koi::SystemRegistry& registry = koi::SystemRegistry::Get();
		registry.Register<PlayerMoverSystem>();
		registry.Register<ObstacleSpawnSystem>();
		registry.Register<ObstacleMoveSystem>();
	}

	void SurfApp::OnInit()
	{
		if (koi::IEditorContext* editor = GetEditorContext())
		{
			editor->RegisterInspector<PlayerMoverInspector>();
			editor->RegisterInspector<ObstacleSpawnerInspector>();
			editor->RegisterInspector<koi::ReflectedInspector<Obstacle>>();
		}
	}
}
