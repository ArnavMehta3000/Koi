#pragma once
#include "Components/Lanes.h"
#include "Koi/Engine/ECS/Observer.h"
#include "Koi/Engine/ECS/Prefab.h"
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Math/Ease.h"
#include "Koi/Engine/Math/Random.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Modules/Physics/Source/PhysicsComponents.h"
#include "Koi/Modules/Physics/Source/PhysicsEvents.h"
#include "Koi/Engine/Core/ReflectSerializer.h"

namespace game
{
	struct Obstacle
	{
		static constexpr std::string_view kKey = "Obstacle";

		koi::f32 Speed       = 10.0f;   // units per second toward +z
		koi::f32 DespawnZ    = 10.0f;
		koi::f32 PopDuration = 0.25f;   // scale-in time
		koi::Transient<koi::f32> Age = 0.0f;

		KOI_REFLECT(Obstacle, Speed, DespawnZ, PopDuration, Age);
	};

	struct ObstaclePrefab : koi::Prefab<ObstaclePrefab,
		koi::MeshRef, koi::MaterialRef, koi::RigidBody, koi::Collider, Obstacle>
	{
		static constexpr std::string_view kKey = "Obstacle";

		static void OnSpawn(Staged& obstacle, const koi::Vector3& at, koi::f32 speed);
	};

	struct ObstacleSpawner
	{
		static constexpr std::string_view kKey = "ObstacleSpawner";

		koi::PrefabRef<ObstaclePrefab> Prefab;
		Lanes         Positions;
		koi::u64      Seed         = 0;      // 0 picks one at boot
		koi::f32      Interval     = 1.0f;   // seconds between spawns at the start
		koi::f32      MinInterval  = 0.35f;
		koi::f32      Speed        = 10.0f;
		koi::f32      MaxSpeed     = 30.0f;
		koi::f32      RampDuration = 60.0f;  // seconds to reach MinInterval / MaxSpeed
		koi::EaseType RampEase     = koi::EaseType::OutQuad;

		// Runtime, not saved
		koi::Transient<koi::Random> Rng;
		koi::Transient<koi::f32>    Elapsed   = 0.0f;
		koi::Transient<koi::f32>    Countdown = 0.0f;

		KOI_REFLECT(ObstacleSpawner, Prefab, Positions, Seed, Interval, MinInterval, Speed, MaxSpeed, RampDuration, RampEase, Rng, Elapsed, Countdown);
	};

	class ObstacleSpawnSystem final : public koi::System<ObstacleSpawner>
	{
	public:
		void Each(const koi::SystemContext& ctx, koi::Entity e, ObstacleSpawner& spawner) override;
	};

	class ObstacleMoveSystem final : public koi::System<koi::Transform, Obstacle>
	{
	public:
		void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, Obstacle& obstacle) override;
	};

	// The obstacle is the sensor
	class ObstacleHitObserver final : public koi::Observer<koi::SensorBegin, const Obstacle>
	{
	public:
		void On(const koi::SensorBegin& event, koi::Entity e, const Obstacle& obstacle) override;
	};
}

namespace koi
{
	static_assert(Serializable<game::Obstacle>);
	static_assert(Serializable<game::ObstacleSpawner>);
}
