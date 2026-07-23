#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Platform/InputCodes.h"

namespace koi
{
	struct Camera;
	class  InputManager;

	enum class CameraMode : u8 { FirstPerson, Orbit };

	class CameraController
	{
	public:
		explicit CameraController(Camera* camera, CameraMode mode = CameraMode::Orbit);

		void Update(f32 dt, const InputManager& input);

		void SetMode(CameraMode mode);
		void ToggleMode();

		// Set initial first-person position and look angles (yaw/pitch in radians)
		void SetFirstPersonPosition(const Vector3& position, f32 yaw = 0.0f, f32 pitch = 0.0f);
		void SetOrbitTarget(const Vector3& target, f32 radius = 5.0f, f32 yaw = 0.0f, f32 pitch = 0.3f);

		[[nodiscard]] CameraMode GetMode()        const noexcept { return m_mode; }
		[[nodiscard]] Vector3    GetOrbitTarget() const noexcept { return m_orbitTarget; }
		[[nodiscard]] f32        GetOrbitRadius() const noexcept { return m_orbitRadius; }

		// Returns true when the controller wants the cursor to be locked (FP mode + RMB)
		[[nodiscard]] bool WantsCursorLock() const noexcept { return m_wantsCursorLock; }

	private:
		void UpdateFirstPerson(f32 dt, const InputManager& input);
		void UpdateOrbit(const InputManager& input);
		void ApplyView();

	public:
		KeyCode ToggleKey      = KeyCode::Tab;

		// First-person
		f32 FpMoveSpeed        = 5.0f;    // units per second
		f32 FpSpeedBoost       = 5.0f;    // multiplier applied when Shift is held
		f32 FpLookSensitivity  = 0.002f;  // radians per pixel
		f32 FpScrollDollySpeed = 10.0f;   // units per scroll tick

		// Orbit
		f32 OrbitSensitivity   = 0.005f;  // radians per pixel (LMB drag)
		f32 OrbitPanSpeed      = 0.005f;  // units per pixel (MMB drag), scales with radius
		f32 OrbitZoomSpeed     = 0.1f;    // fraction of radius per scroll tick
		f32 OrbitMinRadius     = 0.5f;
		f32 OrbitMaxRadius     = 500.0f;

		// Shared pitch clamp (radians) - prevents gimbal lock at the poles
		f32 PitchLimit         = 1.5533f; // ~89 degrees

	private:
		Camera*      m_camera          = nullptr;
		CameraMode   m_mode            = CameraMode::Orbit;
		f32          m_yaw             = 0.0f;
		f32          m_pitch           = 0.0f;
		Vector3      m_fpPosition      = Vector3::Zero;
		Vector3      m_orbitTarget     = Vector3::Zero;
		f32          m_orbitRadius     = 5.0f;
		bool         m_prevToggleKey   = false;
		bool         m_wantsCursorLock = false;
	};
}
