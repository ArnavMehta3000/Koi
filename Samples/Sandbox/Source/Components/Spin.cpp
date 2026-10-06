#include "Components/Spin.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Interop/ImGuiWidgets.h"
#include <imgui.h>
#include <numbers>

namespace
{
	constexpr koi::f32 kDegToRad = static_cast<koi::f32>(std::numbers::pi) / 180.0f;
}

void SpinInspector::Draw(koi::Entity e)
{
	Spin* spin = e.try_get_mut<Spin>();
	if (!spin) return;

	koi::utils::DragAxes("Axis", &spin->Axis.x, 3, 0.01f);
	ImGui::DragFloat("Speed", &spin->Speed, 1.0f, -1440.0f, 1440.0f, "%.1f deg/s");
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
