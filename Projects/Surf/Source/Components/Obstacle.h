#pragma once
#include "Components/Lanes.h"
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/MathSerializer.h"
#include "Koi/Engine/Math/Random.h"
#include "Koi/Engine/Math/Transform.h"
#include <string>

namespace game
{
	struct Obstacle
	{
		static constexpr std::string_view kKey = "game:obstacle";

		koi::f32 Speed       = 10.0f;   // units per second toward +z
		koi::f32 DespawnZ    = 10.0f;
		koi::f32 PopDuration = 0.25f;   // scale-in time
		koi::f32 Age         = 0.0f;
	};

	struct ObstacleSpawner
	{
		static constexpr std::string_view kKey = "game:obstacle_spawner";

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
		koi::Random Rng;
		koi::f32    Elapsed   = 0.0f;
		koi::f32    Countdown = 0.0f;
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
	template<>
	struct Serializer<game::Obstacle>
	{
		static toml::table Serialize(const game::Obstacle& o)
		{
			toml::table out;
			out.insert("speed", ToTomlFloat(o.Speed));
			out.insert("despawn_z", ToTomlFloat(o.DespawnZ));
			out.insert("pop_duration", ToTomlFloat(o.PopDuration));
			return out;
		}

		static std::optional<game::Obstacle> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return game::Obstacle
			{
				.Speed       = (*tbl)["speed"].value_or(10.0f),
				.DespawnZ    = (*tbl)["despawn_z"].value_or(10.0f),
				.PopDuration = (*tbl)["pop_duration"].value_or(0.25f),
			};
		}
	};

	template<>
	struct Serializer<game::ObstacleSpawner>
	{
		static toml::table Serialize(const game::ObstacleSpawner& s)
		{
			toml::table out;
			SerializeInto(out, "Pos0", s.Positions[0]);
			SerializeInto(out, "Pos1", s.Positions[1]);
			SerializeInto(out, "Pos2", s.Positions[2]);
			out.insert("mesh", s.Mesh);
			out.insert("material", s.Material);
			out.insert("seed", static_cast<i64>(s.Seed));
			out.insert("interval", ToTomlFloat(s.Interval));
			out.insert("min_interval", ToTomlFloat(s.MinInterval));
			out.insert("speed", ToTomlFloat(s.Speed));
			out.insert("max_speed", ToTomlFloat(s.MaxSpeed));
			out.insert("ramp_duration", ToTomlFloat(s.RampDuration));
			SerializeInto(out, "ramp_ease", s.RampEase);
			out.insert("despawn_z", ToTomlFloat(s.DespawnZ));
			out.insert("pop_duration", ToTomlFloat(s.PopDuration));
			return out;
		}

		static std::optional<game::ObstacleSpawner> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			game::ObstacleSpawner s;
			s.Positions =
			{
				DeserializeOr(*tbl, "Pos0", Vector3::Zero),
				DeserializeOr(*tbl, "Pos1", Vector3::Zero),
				DeserializeOr(*tbl, "Pos2", Vector3::Zero)
			};
			s.Mesh         = (*tbl)["mesh"].value_or(s.Mesh);
			s.Material     = (*tbl)["material"].value_or(s.Material);
			s.Seed         = static_cast<u64>((*tbl)["seed"].value_or<i64>(0));
			s.Interval     = (*tbl)["interval"].value_or(s.Interval);
			s.MinInterval  = (*tbl)["min_interval"].value_or(s.MinInterval);
			s.Speed        = (*tbl)["speed"].value_or(s.Speed);
			s.MaxSpeed     = (*tbl)["max_speed"].value_or(s.MaxSpeed);
			s.RampDuration = (*tbl)["ramp_duration"].value_or(s.RampDuration);
			s.RampEase     = DeserializeOr(*tbl, "ramp_ease", s.RampEase);
			s.DespawnZ     = (*tbl)["despawn_z"].value_or(s.DespawnZ);
			s.PopDuration  = (*tbl)["pop_duration"].value_or(s.PopDuration);
			s.Rng          = s.Seed != 0 ? Random(s.Seed) : Random();
			return s;
		}
	};

	static_assert(Serializable<game::Obstacle>);
	static_assert(Serializable<game::ObstacleSpawner>);
}
