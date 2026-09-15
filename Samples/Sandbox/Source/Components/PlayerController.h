#pragma once
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/MathSerializer.h"

struct PlayerController
{
	koi::f32 MoveSpeed = 5.0f;   // units per second
	koi::f32 TurnSpeed = 90.0f;  // degrees per second, about the world Y axis
};

namespace koi
{
	template<>
	struct Serializer<PlayerController>
	{
		static toml::table Serialize(const PlayerController& p)
		{
			toml::table out;
			out.insert("move_speed", ToTomlFloat(p.MoveSpeed));
			out.insert("turn_speed", ToTomlFloat(p.TurnSpeed));
			return out;
		}

		static std::optional<PlayerController> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return PlayerController
			{
				.MoveSpeed = (*tbl)["move_speed"].value_or(5.0f),
				.TurnSpeed = (*tbl)["turn_speed"].value_or(90.0f),
			};
		}
	};

	static_assert(Serializable<PlayerController>);
}

class PlayerControllerInspector final : public koi::IComponentInspector
{
public:
	[[nodiscard]] std::string_view Key() const override { return "player_controller"; }
	void Draw(koi::Entity e) override;
};

void RegisterPlayerControllerComponent();
void RegisterPlayerControllerSystem(koi::ECS& ecs);
