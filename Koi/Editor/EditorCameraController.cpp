#include "Koi/Editor/EditorCameraController.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Platform/InputManager.h"
#include <algorithm>
#include <numbers>

namespace koi::editor
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

	namespace
	{
		// Yaw/pitch → unit view direction (right-handed, Y-up): yaw 0 looks down -Z, +yaw turns right
		Vector3 ForwardFromYawPitch(f32 yaw, f32 pitch)
		{
			const f32 cp = std::cos(pitch);
			return Vector3(
				std::sin(yaw) * cp,
				std::sin(pitch),
				-std::cos(yaw) * cp
			);
		}
	}

	CameraController::CameraController(Camera* camera, CameraMode mode)
		: m_camera(camera)
		, m_mode(mode)
	{
		KOI_ASSERT(m_camera, "Null camera passed to CameraController");
	}

	void CameraController::SetFirstPersonPosition(const Vector3& position, f32 yaw, f32 pitch)
	{
		m_fpPosition = position;
		m_yaw        = yaw;
		m_pitch      = std::clamp(pitch, -PitchLimit, PitchLimit);
		if (m_mode == CameraMode::FirstPerson)
			ApplyView();
	}

	void CameraController::SetOrbitTarget(const Vector3& target, f32 radius, f32 yaw, f32 pitch)
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
			Vector3 forward = m_camera->Forward;
			forward.Normalize();
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
		const bool toggleDown = input.GetKeyboard().IsDown(ToggleKey);

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
		const bool rmb    = input.GetMouse().IsButtonDown(kFPVMoveButton);
		m_wantsCursorLock = rmb;

		// Mouse look - only while right mouse is held
		if (rmb)
		{
			const Vector2 delta = input.GetMouse().GetDelta();
			m_yaw   += delta.x * FpLookSensitivity;
			m_pitch -= delta.y * FpLookSensitivity; // invert Y: drag up = look up
			m_pitch  = std::clamp(m_pitch, -PitchLimit, PitchLimit);
			m_yaw    = fmodf(m_yaw, kTwoPi);
		}

		// Recompute local axes from angles
		const f32 cy = std::cos(m_yaw), sy = std::sin(m_yaw);

		Vector3 forward, right;
		ForwardFromYawPitch(m_yaw, m_pitch).Normalize(forward);
		Vector3(cy, 0.0f, sy).Normalize(right);// perpendicular on XZ plane

		const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);

		// WASD + vertical movement
		const f32 speed = FpMoveSpeed
			* (input.GetKeyboard().IsModifierDown(kFPVSpeedModifier) ? FpSpeedBoost : 1.0f)
			* dt;

		if (input.GetKeyboard().IsDown(kFPVMoveForwardKey))  m_fpPosition = m_fpPosition + forward * speed;
		if (input.GetKeyboard().IsDown(kFPVMoveBackwardKey)) m_fpPosition = m_fpPosition - forward * speed;
		if (input.GetKeyboard().IsDown(kFPVMoveRightKey))    m_fpPosition = m_fpPosition + right * speed;
		if (input.GetKeyboard().IsDown(kFPVMoveLeftKey))     m_fpPosition = m_fpPosition - right * speed;

		if (input.GetKeyboard().IsDown(kFPVMoveUpKey) || input.GetKeyboard().IsDown(kFPVMoveUpAltKey))  // Up
			m_fpPosition = m_fpPosition + worldUp * speed;
		if (input.GetKeyboard().IsDown(kFPVMoveDownKey) || input.GetKeyboard().IsDown(kFPVMoveDownAltKey))  // Down
			m_fpPosition = m_fpPosition - worldUp * speed;

		// Scroll = dolly (move along forward axis)
		if (const f32 scroll = input.GetMouse().GetScroll().y; scroll != 0.0f)
			m_fpPosition = m_fpPosition + forward * (scroll * FpScrollDollySpeed);

		ApplyView();
	}

	void CameraController::UpdateOrbit(const InputManager& input)
	{
		m_wantsCursorLock = false;

		const Vector2 delta = input.GetMouse().GetDelta();

		// LMB drag - orbit (yaw / pitch)
		if (input.GetMouse().IsButtonDown(kORBMoveButton))
		{
			m_yaw   += delta.x * OrbitSensitivity;
			m_pitch -= delta.y * OrbitSensitivity;
			m_pitch  = std::clamp(m_pitch, -PitchLimit, PitchLimit);
			m_yaw    = fmodf(m_yaw, kTwoPi);
		}

		// MMB drag - pan (translate target in the camera's local XY plane)
		if (input.GetMouse().IsButtonDown(kORBPanButton))
		{
			// Recompute right/up vectors from view basis
			Vector3 right, upDir;
			m_camera->Right.Normalize(right);
			m_camera->Up.Normalize(upDir);

			const f32 panScale  = m_orbitRadius * OrbitPanSpeed;
			m_orbitTarget = m_orbitTarget - right  * (delta.x * panScale);
			m_orbitTarget = m_orbitTarget + upDir  * (delta.y * panScale);
		}

		// Scroll - zoom by adjusting orbit radius
		if (const f32 scroll = input.GetMouse().GetScroll().y; scroll != 0.0f)
		{
			m_orbitRadius -= scroll * m_orbitRadius * OrbitZoomSpeed;
			m_orbitRadius  = std::clamp(m_orbitRadius, OrbitMinRadius, OrbitMaxRadius);
		}

		ApplyView();
	}

	void CameraController::ApplyView()
	{
		if (!m_camera) return;

		Vector3 position;
		Vector3 target;

		const Vector3 forward = ForwardFromYawPitch(m_yaw, m_pitch);

		if (m_mode == CameraMode::FirstPerson)
		{
			position = m_fpPosition;
			target   = m_fpPosition + forward;
		}
		else
		{
			// Sit behind the target along the view direction, so both modes share yaw/pitch
			position = m_orbitTarget - forward * m_orbitRadius;
			target   = m_orbitTarget;
		}

		m_camera->SetView(position, target);
	}
}
