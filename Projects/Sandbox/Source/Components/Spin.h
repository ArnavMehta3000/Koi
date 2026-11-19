#pragma once
#include "Koi/Engine/ECS/System.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"

// A component the engine has never heard of: defined, serialized, inspected and simulated entirely from game code
struct Spin
{
	static constexpr std::string_view kKey = "Spin";

	koi::Vector3 Axis  = koi::Vector3(0.0f, 1.0f, 0.0f);
	koi::f32     Speed = 90.0f;  // degrees per second, about Axis in world space

	KOI_REFLECT(Spin, Axis, Speed);
};

class SpinSystem final : public koi::System<koi::Transform, const Spin>
{
public:
	void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& xform, const Spin& spin) override;
};

void RegisterSpinComponent();
void RegisterSpinSystem();
