#pragma once
#include "Components/Lanes.h"
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/Ease.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"

namespace game
{
	struct PlayerMover
	{
		static constexpr std::string_view kKey = "game:player_mover";
		static constexpr bool kReflect = true;

		Lanes         Positions;
		koi::f32      Duration = 0.15f;  // seconds per lane switch
		koi::EaseType Ease     = koi::EaseType::OutBack;

		// Runtime tween state, not saved
		rfl::Skip<koi::i32>     Lane = 1;
		rfl::Skip<koi::Vector3> From;
		rfl::Skip<koi::f32>     T    = 1.0f;
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
	static_assert(Serializable<game::PlayerMover>);
}
