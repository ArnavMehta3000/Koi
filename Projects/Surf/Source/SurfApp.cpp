#include "SurfApp.h"
#include "Components/Obstacle.h"
#include "Components/PlayerMover.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/ObserverRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Logging/Logger.h"

KOI_LOG_CATEGORY(Surf);

std::unique_ptr<koi::AppBase> koi::AppBase::Create([[maybe_unused]] koi::CommandLine& cmd)
{
	return std::make_unique<game::SurfApp>();
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

		koi::ObserverRegistry::Get().Register<ObstacleHitObserver>();
	}

	void SurfApp::OnTick(koi::f32 /*dt*/)
	{
		if (!GetWorld().GetECS().Has<RestartRequested>()) return;

		KOI_LOG(Surf, Info, "Player hit an obstacle, restarting");
		LoadScene("Main");
	}
}
