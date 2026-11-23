#pragma once
#include "Koi/Editor/EditorCameraController.h"
#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Engine/Graphics/Camera.h"

namespace koi::editor
{
	class RenderViewport : public ViewportPanel
	{
	public:
		[[nodiscard]] inline const Camera& GetCamera() const { return m_camera; }

		void FrameBounds(const BoundingBox& bounds);

		// The orbit camera, under [camera]
		void SaveState(toml::table& state) const override;
		void LoadState(const toml::table& state) override { LoadCamera(state); }

	protected:
		RenderViewport();

		bool LoadCamera(const toml::table& state);
		void UpdateCamera(f32 dt, const InputManager& input, bool blocked);
		void OnTargetResized(u32 width, u32 height) override;

	protected:
		Camera           m_camera;
		CameraController m_controller;
	};
}
