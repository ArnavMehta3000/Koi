#include "Koi/Editor/EditorCameraController.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Math/Math.h"
#include <algorithm>
#include <numbers>
#include <cassert>

namespace koi
{
	static constexpr f32 kPi                     = std::numbers::pi_v<f32>;
	static constexpr f32 kTwoPi                  = 2.0f * kPi;
	static constexpr MouseButton kFPVMoveButton  = MouseButton::Right;
	static constexpr Modifier kFPVSpeedModifier  = Modifier::Shift;
	static constexpr KeyCode kFPVMoveForwardKey  = KeyCode::W;
	static constexpr KeyCode kFPVMoveBackwardKey = KeyCode::S;
	static constexpr KeyCode kFPVMoveRightKey    = KeyCode::D;
	static constexpr KeyCode kFPVMoveLeftKey     = KeyCode::A;
	static constexpr KeyCode kFPVMoveUpKey       = KeyCode::E;
	static constexpr KeyCode kFPVMoveDownKey     = KeyCode::Q;
	static constexpr KeyCode kFPVMoveUpAltKey    = KeyCode::Space;
	static constexpr KeyCode kFPVMoveDownAltKey  = KeyCode::LeftControl;

	static constexpr MouseButton kORBMoveButton = MouseButton::Left;
	static constexpr MouseButton kORBPanButton = MouseButton::Middle;

	// Spherical → cartesian offset for the orbit camera (left-handed, Y-up)
	static math::float3 SphericalOffset(f32 yaw, f32 pitch, f32 radius)
	{
		const f32 cp = cosf(pitch);
		return math::float3(
			radius * sinf(yaw) * cp,
			radius * sinf(pitch),
			radius * cosf(yaw) * cp
		);
	}

	CameraController::CameraController(Camera* camera, CameraMode mode)
		: m_camera(camera)
		, m_mode(mode)
	{
		assert(m_camera && "Null camera passed to CameraController");
	}

	void CameraController::SetFirstPersonPosition(math::float3 position, f32 yaw, f32 pitch)
	{
		m_fpPosition = position;
		m_yaw        = yaw;
		m_pitch      = std::clamp(pitch, -PitchLimit, PitchLimit);
		if (m_mode == CameraMode::FirstPerson)
			ApplyView();
	}

	void CameraController::SetOrbitTarget(math::float3 target, f32 radius, f32 yaw, f32 pitch)
	{
		m_orbitTarget = target;
		m_orbitRadius = std::clamp(radius, OrbitMinRadius, OrbitMaxRadius);
		m_yaw         = yaw;
		m_pitch       = std::clamp(pitch, -PitchLimit, PitchLimit);
		if (m_mode == CameraMode::Orbit)
			ApplyView();
	}

	void CameraController::SetMode(CameraMode mode)
	{
		if (m_mode == mode) return;

		if (mode == CameraMode::Orbit)
		{
			const math::float3 forward = math::normalize(m_camera->Forward);
			m_orbitTarget = m_fpPosition + forward * m_orbitRadius;
		}
		else
		{
			m_fpPosition = m_camera->Position;
		}

		m_mode = mode;
		ApplyView();
	}

	void CameraController::ToggleMode()
	{
		SetMode(m_mode == CameraMode::FirstPerson ? CameraMode::Orbit : CameraMode::FirstPerson);
	}

	void CameraController::Update(f32 dt, const InputManager& input)
	{
		const bool toggleDown = input.IsKeyDown(ToggleKey);

		if (toggleDown && !m_prevToggleKey)
			ToggleMode();

		m_prevToggleKey = toggleDown;

		if (m_mode == CameraMode::FirstPerson)
			UpdateFirstPerson(dt, input);
		else
			UpdateOrbit(input);
	}


	void CameraController::UpdateFirstPerson(f32 dt, const InputManager& input)
	{
		using namespace math;

		const bool rmb    = input.IsMouseButtonDown(kFPVMoveButton);
		m_wantsCursorLock = rmb;

		// Mouse look - only while right mouse is held
		if (rmb)
		{
			const float2 delta = input.GetMouseDelta();
			m_yaw   += delta.x * FpLookSensitivity;
			m_pitch -= delta.y * FpLookSensitivity; // invert Y: drag up = look up
			m_pitch  = std::clamp(m_pitch, -PitchLimit, PitchLimit);
			m_yaw    = fmodf(m_yaw, kTwoPi);
		}

		// Recompute local axes from angles
		const f32 cp = cosf(m_pitch), sp = sinf(m_pitch);
		const f32 cy = cosf(m_yaw),   sy = sinf(m_yaw);

		const float3 forward = normalize(float3(sy * cp, sp, cy * cp));
		const float3 right   = normalize(float3(cy, 0.0f, -sy)); // perpendicular on XZ plane
		const float3 worldUp = float3(0.0f, 1.0f, 0.0f);

		// WASD + vertical movement
		const f32 speed = FpMoveSpeed
			* (input.IsModifierDown(kFPVSpeedModifier) ? FpSpeedBoost : 1.0f)
			* dt;

		if (input.IsKeyDown(kFPVMoveForwardKey))  m_fpPosition = m_fpPosition + forward * speed;
		if (input.IsKeyDown(kFPVMoveBackwardKey)) m_fpPosition = m_fpPosition - forward * speed;
		if (input.IsKeyDown(kFPVMoveRightKey))    m_fpPosition = m_fpPosition + right * speed;
		if (input.IsKeyDown(kFPVMoveLeftKey))     m_fpPosition = m_fpPosition - right * speed;

		if (input.IsKeyDown(kFPVMoveUpKey) || input.IsKeyDown(kFPVMoveUpAltKey))  // Up
			m_fpPosition = m_fpPosition + worldUp * speed;
		if (input.IsKeyDown(kFPVMoveDownKey) || input.IsKeyDown(kFPVMoveDownAltKey))  // Down
			m_fpPosition = m_fpPosition - worldUp * speed;

		// Scroll = dolly (move along forward axis)
		if (const f32 scroll = input.GetScrollDelta().y; scroll != 0.0f)
			m_fpPosition = m_fpPosition + forward * (scroll * FpScrollDollySpeed);

		ApplyView();
	}

	void CameraController::UpdateOrbit(const InputManager& input)
	{
		using namespace math;

		m_wantsCursorLock = false;

		const float2 delta = input.GetMouseDelta();

		// LMB drag - orbit (yaw / pitch)
		if (input.IsMouseButtonDown(kORBMoveButton))
		{
			m_yaw   += delta.x * OrbitSensitivity;
			m_pitch -= delta.y * OrbitSensitivity;
			m_pitch  = std::clamp(m_pitch, -PitchLimit, PitchLimit);
			m_yaw    = fmodf(m_yaw, kTwoPi);
		}

		// MMB drag - pan (translate target in the camera's local XY plane)
		if (input.IsMouseButtonDown(kORBPanButton))
		{
			// Recompute right/up vectors from view basis
			const float3 right  = normalize(m_camera->Right);
			const float3 upDir  = normalize(m_camera->Up);
			const f32 panScale  = m_orbitRadius * OrbitPanSpeed;
			m_orbitTarget = m_orbitTarget - right  * (delta.x * panScale);
			m_orbitTarget = m_orbitTarget + upDir  * (delta.y * panScale);
		}

		// Scroll - zoom by adjusting orbit radius
		if (const f32 scroll = input.GetScrollDelta().y; scroll != 0.0f)
		{
			m_orbitRadius -= scroll * m_orbitRadius * OrbitZoomSpeed;
			m_orbitRadius  = std::clamp(m_orbitRadius, OrbitMinRadius, OrbitMaxRadius);
		}

		ApplyView();
	}

	void CameraController::ApplyView()
	{
		if (!m_camera) return;

		math::float3 position;
		math::float3 target;

		if (m_mode == CameraMode::FirstPerson)
		{
			const f32 cp = cosf(m_pitch), sp = sinf(m_pitch);
			const f32 cy = cosf(m_yaw),   sy = sinf(m_yaw);
			const math::float3 forward = math::normalize(math::float3(sy * cp, sp, cy * cp));

			position = m_fpPosition;
			target   = m_fpPosition + forward;
		}
		else
		{
			position = m_orbitTarget + SphericalOffset(m_yaw, m_pitch, m_orbitRadius);
			target   = m_orbitTarget;
		}

		m_camera->SetView(position, target);
	}
}
