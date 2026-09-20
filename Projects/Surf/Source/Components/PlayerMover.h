#pragma once
#include "Components/Lanes.h"
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/MathSerializer.h"
#include "Koi/Engine/Math/Transform.h"

namespace game
{
	struct PlayerMover
	{
		static constexpr std::string_view kKey = "game:player_mover";

		Lanes         Positions;
		koi::f32      Duration = 0.15f;  // seconds per lane switch
		koi::EaseType Ease     = koi::EaseType::OutBack;

		// Runtime tween state, not saved
		koi::i32     Lane = 1;
		koi::Vector3 From;
		koi::f32     T    = 1.0f;
	};

	class PlayerMoverInspector final : public koi::IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return PlayerMover::kKey; }
		void Draw(koi::Entity e) override;
	};

	class PlayerMoverSystem final : public koi::System<koi::Transform, PlayerMover>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "PlayerMover"; }
		void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, PlayerMover& mover) override;
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
			SerializeInto(out, "Pos0", p.Positions[0]);
			SerializeInto(out, "Pos1", p.Positions[1]);
			SerializeInto(out, "Pos2", p.Positions[2]);
			out.insert("duration", ToTomlFloat(p.Duration));
			SerializeInto(out, "ease", p.Ease);
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
					DeserializeOr(*tbl, "Pos0", Vector3::Zero),
					DeserializeOr(*tbl, "Pos1", Vector3::Zero),
					DeserializeOr(*tbl, "Pos2", Vector3::Zero)
				},
				.Duration = (*tbl)["duration"].value_or(0.15f),
				.Ease     = DeserializeOr(*tbl, "ease", EaseType::OutBack),
			};
		}
	};

	static_assert(Serializable<game::PlayerMover>);
}
