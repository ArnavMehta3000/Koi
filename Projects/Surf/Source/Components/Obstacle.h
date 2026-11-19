#pragma once
#include "Components/Lanes.h"
#include "Koi/Engine/ECS/Observer.h"
#include "Koi/Engine/ECS/Prefab.h"
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Math/Ease.h"
#include "Koi/Engine/Math/Random.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include "Koi/Engine/Physics/PhysicsEvents.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"

namespace game
{
	struct Obstacle
	{
		static constexpr std::string_view kKey = "Obstacle";
		static constexpr bool kReflect = true;

		koi::f32 Speed       = 10.0f;   // units per second toward +z
		koi::f32 DespawnZ    = 10.0f;
		koi::f32 PopDuration = 0.25f;   // scale-in time
		rfl::Skip<koi::f32> Age = 0.0f;
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
		static constexpr bool kReflect = true;

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
		rfl::Skip<koi::Random> Rng;
		rfl::Skip<koi::f32>    Elapsed   = 0.0f;
		rfl::Skip<koi::f32>    Countdown = 0.0f;
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
