#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Editor/EditorCameraController.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	class Renderer;
	class CommandList;
	class InputManager;

	enum class ViewportCamera : u8
	{
		Scene,
		Game,
	};

	enum class ViewportResolution : u8
	{
		FreeAspect,  // track the panel, no letterboxing
		Window,      // what a standalone build would render
		R1920x1080,
		R1280x720,
	};

	class ViewportPanel : public IEditorPanel
	{
	public:
		explicit ViewportPanel(Renderer& renderer);
		~ViewportPanel() override;

		[[nodiscard]] std::string_view GetName() const override { return "Viewport"; }
		void OnDraw() override;

		[[nodiscard]] inline const ViewportTarget* GetTarget() const { return m_valid ? &m_target : nullptr; }
		[[nodiscard]] inline const Camera& GetCamera() const { return m_camera; }
		[[nodiscard]] inline bool IsGameCamera() const { return m_cameraMode == ViewportCamera::Game; }
		[[nodiscard]] inline ViewportCamera GetCameraMode() const { return m_cameraMode; }
		inline void SetCameraMode(ViewportCamera mode) { m_cameraMode = mode; }

		const ViewportTarget* PrepareFrame(CommandList& cmd);
		void FrameBounds(const AABB& bounds);
		void Update(f32 dt, const InputManager& input);

	private:
		void EnsureSize(u32 width, u32 height);
		void DrawToolbar();

	private:
		Renderer*         m_renderer;
		ViewportTarget    m_target{};
		bool              m_valid           = false;
		u32               m_desiredWidth    = 1280;
		u32               m_desiredHeight   = 720;

		ViewportCamera     m_cameraMode = ViewportCamera::Scene;
		ViewportResolution m_resolution = ViewportResolution::FreeAspect;

		Camera            m_camera;
		CameraController  m_controller;
		bool              m_isHovered        = false;
		bool              m_isCapturingInput = false;
	};
}
