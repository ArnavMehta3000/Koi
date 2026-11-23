#include "Koi/Editor/Panels/RenderViewport.h"
#include "Koi/Engine/Core/Serializer.h"
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Math/MathSerializer.h"

namespace koi::editor
{
	static constexpr f32 kFovY = 0.8f;
	static constexpr f32 kNear = 0.05f;
	static constexpr f32 kFar  = 1000.0f;

	RenderViewport::RenderViewport()
		: m_controller(&m_camera, CameraMode::Orbit)
	{
	}

	void RenderViewport::FrameBounds(const BoundingBox& bounds)
	{
		const Vector3 center(bounds.Center);
		const f32 radius = math::BoundsRadius(bounds);

		m_camera.SetProjection(kFovY, m_camera.Aspect, kNear, kFar);
		m_controller.SetOrbitTarget(center, radius * 2.5f, 0.0f, -0.5f);
	}

	void RenderViewport::SaveState(toml::table& state) const
	{
		toml::table camera;
		koi::utils::SerializeInto(camera, "target", m_controller.GetOrbitTarget());
		camera.insert("radius", m_controller.GetOrbitRadius());
		camera.insert("yaw",    m_controller.GetYaw());
		camera.insert("pitch",  m_controller.GetPitch());
		state.insert("camera", std::move(camera));
	}

	bool RenderViewport::LoadCamera(const toml::table& state)
	{
		const toml::table* camera = state["camera"].as_table();
		if (!camera) return false;

		m_camera.SetProjection(kFovY, m_camera.Aspect, kNear, kFar);
		m_controller.SetOrbitTarget(
			koi::utils::DeserializeOr(*camera, "target", Vector3::Zero),
			(*camera)["radius"].value_or(10.0f),
			(*camera)["yaw"].value_or(0.0f),
			(*camera)["pitch"].value_or(-0.5f));
		return true;
	}

	void RenderViewport::UpdateCamera(f32 dt, const InputManager& input, bool blocked)
	{
		if (!blocked && (IsHovered() || IsCapturing()))
		{
			m_controller.Update(dt, input);
		}
	}

	void RenderViewport::OnTargetResized(u32 width, u32 height)
	{
		m_camera.SetAspect(static_cast<f32>(width) / static_cast<f32>(height));
	}
}
