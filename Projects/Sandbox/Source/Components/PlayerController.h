#pragma once
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"

struct PlayerController
{
	static constexpr std::string_view kKey = "PlayerController";
	static constexpr bool kReflect = true;

	koi::f32 MoveSpeed = 5.0f;   // units per second
	koi::f32 TurnSpeed = 90.0f;  // degrees per second, about the world Y axis
};

class PlayerControllerSystem final : public koi::System<koi::Transform, const PlayerController>
{
public:
	void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& xform, const PlayerController& controller) override;
};

void RegisterPlayerControllerComponent();
void RegisterPlayerControllerSystem();
