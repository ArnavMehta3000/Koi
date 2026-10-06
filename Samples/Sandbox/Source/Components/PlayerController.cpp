#include "Components/PlayerController.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Math/Scalar.h"

void PlayerControllerSystem::Each(const koi::SystemContext& ctx, koi::Entity, koi::Transform& xform, const PlayerController& controller)
{
	const koi::InputManager& input = koi::Engine::Get().GetSubsystem<koi::InputManager>();
	const koi::f32 dt = ctx.DeltaTime;

	// Top down movement
	const koi::Vector3 move(input.GetAction("MoveX"), 0.0f, -input.GetAction("MoveZ"));
	xform.Position = xform.Position + move * (controller.MoveSpeed * dt);

	const koi::f32 turn = input.GetAction("Turn");
	if (turn != 0.0f)
	{
		const koi::Quaternion delta = koi::Quaternion::CreateFromAxisAngle(koi::Vector3::Up, koi::math::DegToRad(-turn * controller.TurnSpeed * dt));
		xform.Rotation = koi::Quaternion::Concatenate(xform.Rotation, delta);
	}
}

void RegisterPlayerControllerComponent()
{
	koi::ComponentRegistry::Get().Register<koi::ComponentType<PlayerController>>(51);
}

void RegisterPlayerControllerSystem()
{
	koi::SystemRegistry::Get().Register<PlayerControllerSystem>();
}
