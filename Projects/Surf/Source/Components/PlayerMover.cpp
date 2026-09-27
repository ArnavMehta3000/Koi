#include "Components/PlayerMover.h"
#include "Koi/Engine/Interop/ReflectInspector.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <imgui.h>

namespace game
{
	void PlayerMoverInspector::Draw(koi::Entity e)
	{
		PlayerMover* mover = e.try_get_mut<PlayerMover>();
		if (!mover) return;

		DrawLaneSpheres(mover->Positions, *mover->Lane);
		koi::DrawReflected(*mover);
	}

	void PlayerMoverSystem::Each(const koi::SystemContext& ctx, koi::Entity, koi::Transform& transform, PlayerMover& mover)
	{
		const koi::InputManager& input = koi::Engine::Get().GetSubsystem<koi::InputManager>();

		const koi::i32 lane = std::clamp(
			*mover.Lane - (input.IsActionPressed("MoveLeft") ? 1 : 0) + (input.IsActionPressed("MoveRight") ? 1 : 0),
			0, static_cast<koi::i32>(mover.Positions.size()) - 1);

		if (lane != *mover.Lane)
		{
			mover.Lane = lane;
			mover.From = transform.Position;
			mover.T    = 0.0f;
		}

		if (*mover.T >= 1.0f) return;

		mover.T = std::min(*mover.T + ctx.DeltaTime / std::max(mover.Duration, 1e-3f), 1.0f);
		transform.Position = koi::Vector3::Lerp(*mover.From, mover.Positions[static_cast<size_t>(*mover.Lane)], koi::Ease(mover.Ease, *mover.T));
	}
}
