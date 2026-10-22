#include "Components/Spin.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include <numbers>

namespace
{
	constexpr koi::f32 kDegToRad = static_cast<koi::f32>(std::numbers::pi) / 180.0f;
}

void SpinSystem::Each(const koi::SystemContext& ctx, koi::Entity, koi::Transform& xform, const Spin& spin)
{
	if (spin.Axis.LengthSquared() < 1e-8f) return;

	koi::Vector3 axis = spin.Axis;
	axis.Normalize();

	const koi::Quaternion delta = koi::Quaternion::CreateFromAxisAngle(axis, spin.Speed * kDegToRad * ctx.DeltaTime);
	xform.Rotation = koi::Quaternion::Concatenate(xform.Rotation, delta);
}

void RegisterSpinComponent()
{
	koi::ComponentRegistry::Get().Register<koi::ComponentType<Spin>>(50);
}

void RegisterSpinSystem()
{
	koi::SystemRegistry::Get().Register<SpinSystem>();
}
