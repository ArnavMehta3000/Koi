#include "Components/Obstacle.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/Interop/ReflectInspector.h"
#include "Koi/Engine/Math/Scalar.h"
#include <algorithm>

namespace game
{
	void ObstacleSpawnerInspector::Draw(koi::Entity e)
	{
		ObstacleSpawner* s = e.try_get_mut<ObstacleSpawner>();
		if (!s) return;

		DrawLaneSpheres(s->Positions, -1);
		koi::utils::DrawReflected(*s);
	}

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
			.With(Obstacle{ .Speed = speed, .DespawnZ = spawner.DespawnZ, .PopDuration = spawner.PopDuration });
	}

	void ObstacleMoveSystem::Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, Obstacle& obstacle)
	{
		*obstacle.Age += ctx.DeltaTime;
		transform.Position.z += obstacle.Speed * ctx.DeltaTime;
		transform.Scale = koi::Vector3::One * koi::math::Ease(koi::EaseType::OutBack, *obstacle.Age / std::max(obstacle.PopDuration, 1e-3f));

		if (transform.Position.z > obstacle.DespawnZ)
		{
			e.destruct();
		}
	}
}
