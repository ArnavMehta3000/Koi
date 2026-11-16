#include "Components/Obstacle.h"
#include "Components/PlayerMover.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/Math/Scalar.h"
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include <algorithm>

KOI_LOG_CATEGORY(ObstacleSpawner);

namespace game
{
	void ObstacleSpawnSystem::Each(const koi::SystemContext& ctx, koi::Entity, ObstacleSpawner& spawner)
	{
		if (*spawner.Elapsed == 0.0f && spawner.Seed != 0) spawner.Rng = koi::Random(spawner.Seed);

		*spawner.Elapsed   += ctx.DeltaTime;
		*spawner.Countdown -= ctx.DeltaTime;
		if (*spawner.Countdown > 0.0f) return;

		const koi::f32 ramp     = koi::math::Ease(spawner.RampEase, *spawner.Elapsed / std::max(spawner.RampDuration, 1e-3f));
		const koi::f32 interval = koi::math::Lerp(spawner.Interval, spawner.MinInterval, ramp);
		const koi::f32 speed    = koi::math::Lerp(spawner.Speed, spawner.MaxSpeed, ramp);
		spawner.Countdown = interval;

		const koi::i32 lane = (*spawner.Rng).Range(0, static_cast<koi::i32>(spawner.Positions.size()) - 1);

		ctx.World.Spawn()
			.At(spawner.Positions[static_cast<size_t>(lane)])
			.Scaled(0.0f)
			.WithMesh(spawner.Mesh)
			.WithMaterial(spawner.Material)
			.With(Obstacle{ .Speed = speed, .DespawnZ = spawner.DespawnZ, .PopDuration = spawner.PopDuration })
			.With(koi::RigidBody{ .Type = koi::BodyType::Kinematic })  // ObstacleMoveSystem drives the Transform
			.With(koi::Collider{ .IsSensor = true });
	}

	void ObstacleMoveSystem::Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, Obstacle& obstacle)
	{
		*obstacle.Age += ctx.DeltaTime;
		transform.Position.z += obstacle.Speed * ctx.DeltaTime;
		transform.Scale = koi::Vector3::One * koi::math::Ease(koi::EaseType::OutBack, *obstacle.Age / std::max(obstacle.PopDuration, 1e-3f));

		if (transform.Position.z > obstacle.DespawnZ)
		{
			e.Destroy();
		}
	}

	void ObstacleHitObserver::On(const koi::SensorBegin& event, koi::Entity e, const Obstacle& obstacle)
	{
		const std::string other = event.Other ? std::string(event.Other.GetName()) : std::string("<destroyed>");
		const koi::Vector3& at  = e.Get<const koi::Transform>().Position;

		KOI_LOG(ObstacleSpawner, Info, "Obstacle {} hit '{}' at ({:.2f}, {:.2f}, {:.2f}), speed {:.1f}, age {:.2f}s",
			e.GetId(), other, at.x, at.y, at.z, obstacle.Speed, *obstacle.Age);

		if (event.Other && event.Other.Has<PlayerMover>())
		{
			KOI_LOG(ObstacleSpawner, Info, "Player hit an obstacle, restarting");
			koi::AppBase::Get().LoadScene("Main");
		}
	}
}
