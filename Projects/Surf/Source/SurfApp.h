#pragma once
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/MathSerializer.h"
#include "Koi/Engine/Runtime/AppBase.h"

namespace game
{
	struct PlayerMover
	{
		std::array<koi::Vector3, 3> Positions;  // Valid locations for the player
		koi::f32 Speed = 12.0f;                 // units per second toward the target lane
		koi::i32 Lane  = 1;                     // index into Positions; not saved
	};

	class PlayerMoverInspector final : public koi::IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return "game:player_mover"; }
		void Draw(koi::Entity e) override;
	};

	class PlayerMoverSystem final : public koi::System<koi::Transform, PlayerMover>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "PlayerMover"; }
		void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, PlayerMover& mover) override;
	};

	class SurfApp : public koi::AppBase
	{
	public:
		void OnRegisterComponents() override;
		void OnRegisterSystems() override;
		void OnInit() override;
		void OnTick(koi::f32 dt) override;
	};
}

namespace koi
{
	template<>
	struct Serializer<game::PlayerMover>
	{
		static toml::table Serialize(const game::PlayerMover& p)
		{
			toml::table out;
			koi::SerializeInto(out, "Pos0", p.Positions[0]);
			koi::SerializeInto(out, "Pos1", p.Positions[1]);
			koi::SerializeInto(out, "Pos2", p.Positions[2]);
			out.insert("speed", ToTomlFloat(p.Speed));
			return out;
		}

		static std::optional<game::PlayerMover> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return game::PlayerMover
			{
				.Positions =
				{
					koi::DeserializeOr(*tbl, "Pos0", Vector3::Zero),
					koi::DeserializeOr(*tbl, "Pos1", Vector3::Zero),
					koi::DeserializeOr(*tbl, "Pos2", Vector3::Zero)
				},
				.Speed = (*tbl)["speed"].value_or(12.0f),
			};
		}
	};

	static_assert(Serializable<game::PlayerMover>);
}
