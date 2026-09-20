#include "Components/PlayerController.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <numbers>

namespace
{
	constexpr koi::f32 kDegToRad = static_cast<koi::f32>(std::numbers::pi) / 180.0f;
}

void PlayerControllerInspector::Draw(koi::Entity e)
{
	PlayerController* controller = e.try_get_mut<PlayerController>();
	if (!controller) return;

	ImGui::DragFloat("Move Speed", &controller->MoveSpeed, 0.1f, 0.0f, 100.0f, "%.1f u/s");
	ImGui::DragFloat("Turn Speed", &controller->TurnSpeed, 1.0f, 0.0f, 720.0f, "%.0f deg/s");
}

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
		const koi::Quaternion delta = koi::Quaternion::CreateFromAxisAngle(
			koi::Vector3::Up, -turn * controller.TurnSpeed * kDegToRad * dt);
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
