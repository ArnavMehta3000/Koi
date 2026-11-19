#pragma once
#include "Components/Lanes.h"
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Math/Ease.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"

namespace game
{
	struct PlayerMover
	{
		static constexpr std::string_view kKey = "PlayerMover";

		Lanes         Positions;
		koi::f32      Duration = 0.15f;  // seconds per lane switch
		koi::EaseType Ease     = koi::EaseType::OutBack;

		// Runtime tween state, not saved
		koi::Transient<koi::i32>     Lane = 1;
		koi::Transient<koi::Vector3> From;
		koi::Transient<koi::f32>     T    = 1.0f;

		KOI_REFLECT(PlayerMover, Positions, Duration, Ease, Lane, From, T);
	};

	class PlayerMoverSystem final : public koi::System<koi::Transform, PlayerMover>
	{
	public:
		void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, PlayerMover& mover) override;
	};
}

namespace koi
{
	static_assert(Serializable<game::PlayerMover>);
}
