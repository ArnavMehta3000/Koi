#pragma once
#include "Components/Lanes.h"
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/Ease.h"
#include "Koi/Engine/Math/Random.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include <string>

namespace game
{
	struct Obstacle
	{
		static constexpr std::string_view kKey = "game:obstacle";
		static constexpr bool kReflect = true;

		koi::f32 Speed       = 10.0f;   // units per second toward +z
		koi::f32 DespawnZ    = 10.0f;
		koi::f32 PopDuration = 0.25f;   // scale-in time
		rfl::Skip<koi::f32> Age = 0.0f;
	};

	struct ObstacleSpawner
	{
		static constexpr std::string_view kKey = "game:obstacle_spawner";
		static constexpr bool kReflect = true;

		Lanes         Positions;
		std::string   Mesh         = "engine://Content/Meshes/Box.gltf";
		std::string   Material     = "project://Content/Materials/Obstacle.mat.toml";
		koi::u64      Seed         = 0;      // 0 picks one at boot
		koi::f32      Interval     = 1.0f;   // seconds between spawns at the start
		koi::f32      MinInterval  = 0.35f;
		koi::f32      Speed        = 10.0f;
		koi::f32      MaxSpeed     = 30.0f;
		koi::f32      RampDuration = 60.0f;  // seconds to reach MinInterval / MaxSpeed
		koi::EaseType RampEase     = koi::EaseType::OutQuad;
		koi::f32      DespawnZ     = 10.0f;
		koi::f32      PopDuration  = 0.25f;

		// Runtime, not saved
		rfl::Skip<koi::Random> Rng;
		rfl::Skip<koi::f32>    Elapsed   = 0.0f;
		rfl::Skip<koi::f32>    Countdown = 0.0f;
	};

	class ObstacleSpawnerInspector final : public koi::IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return ObstacleSpawner::kKey; }
		void Draw(koi::Entity e) override;
	};

	class ObstacleSpawnSystem final : public koi::System<ObstacleSpawner>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "ObstacleSpawn"; }
		void Each(const koi::SystemContext& ctx, koi::Entity e, ObstacleSpawner& spawner) override;
	};

	class ObstacleMoveSystem final : public koi::System<koi::Transform, Obstacle>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "ObstacleMove"; }
		void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, Obstacle& obstacle) override;
	};
}

namespace koi
{
	static_assert(Serializable<game::Obstacle>);
	static_assert(Serializable<game::ObstacleSpawner>);
}
