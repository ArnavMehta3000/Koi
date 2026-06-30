module;
#include <hlsl++.h>
export module Koi.Graphics.CameraController;

import Koi.Core.Types;
import Koi.Core.Math;
import Koi.Platform.InputCodes;
import Koi.Graphics.Camera;

export namespace koi
{
	class InputManager;

	enum class CameraMode : u8 { FirstPerson, Orbit };

	class CameraController
	{
	public:
		explicit CameraController(Camera* camera, CameraMode mode = CameraMode::Orbit);

		void Update(f32 dt, const InputManager& input);
		void SetMode(CameraMode mode);
		void ToggleMode();

		void SetFirstPersonPosition(math::float3 position, f32 yaw = 0.0f, f32 pitch = 0.0f);
		void SetOrbitTarget(math::float3 target, f32 radius = 5.0f, f32 yaw = 0.0f, f32 pitch = 0.3f);

		[[nodiscard]] CameraMode   GetMode()        const noexcept { return m_mode;        }
		[[nodiscard]] math::float3 GetOrbitTarget() const noexcept { return m_orbitTarget; }
		[[nodiscard]] f32          GetOrbitRadius() const noexcept { return m_orbitRadius; }
		[[nodiscard]] bool         WantsCursorLock() const noexcept { return m_wantsCursorLock; }

	public:
		KeyCode ToggleKey          = KeyCode::Tab;
		f32 FpMoveSpeed            = 5.0f;
		f32 FpSpeedBoost           = 3.0f;
		f32 FpLookSensitivity      = 0.002f;
		f32 FpScrollDollySpeed     = 10.0f;
		f32 OrbitSensitivity       = 0.005f;
		f32 OrbitPanSpeed          = 0.005f;
		f32 OrbitZoomSpeed         = 0.1f;
		f32 OrbitMinRadius         = 0.5f;
		f32 OrbitMaxRadius         = 500.0f;
		f32 PitchLimit             = 1.5533f;

	private:
		void UpdateFirstPerson(f32 dt, const InputManager& input);
		void UpdateOrbit(const InputManager& input);
		void ApplyView();

	private:
		Camera*      m_camera          = nullptr;
		CameraMode   m_mode            = CameraMode::Orbit;
		f32          m_yaw             = 0.0f;
		f32          m_pitch           = 0.0f;
		math::float3 m_fpPosition{};
		math::float3 m_orbitTarget{};
		f32          m_orbitRadius     = 5.0f;
		bool         m_prevToggleKey   = false;
		bool         m_wantsCursorLock = false;
	};
}
