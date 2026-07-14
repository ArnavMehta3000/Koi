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

	class ViewportPanel : public IEditorPanel
	{
	public:
		explicit ViewportPanel(Renderer& renderer);
		~ViewportPanel() override;

		[[nodiscard]] std::string_view GetName() const override { return "Viewport"; }
		void OnDraw() override;

		[[nodiscard]] inline const ViewportTarget* GetTarget() const { return m_valid ? &m_target : nullptr; }
		[[nodiscard]] inline const Camera& GetCamera() const { return m_camera; }

		const ViewportTarget* PrepareFrame(CommandList& cmd);
		void FrameBounds(const AABB& bounds);
		void Update(f32 dt, const InputManager& input);

	private:
		void EnsureSize(u32 width, u32 height);

	private:
		Renderer*         m_renderer;
		ViewportTarget    m_target{};
		bool              m_valid           = false;
		u32               m_desiredWidth    = 1280;
		u32               m_desiredHeight   = 720;

		Camera            m_camera;
		CameraController  m_controller;
		bool              m_isHovered        = false;
		bool              m_isCapturingInput = false;
	};
}
