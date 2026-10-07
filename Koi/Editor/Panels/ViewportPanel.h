#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Editor/EditorCameraController.h"
#include <IconsFontAwesome6.h>

namespace toml { inline namespace v3 { class table; } }

namespace koi
{
	class Renderer;
	class CommandList;
	class InputManager;
	class AppBase;
}

namespace koi::editor
{
	class Selection;

	enum class GizmoOp : u8
	{
		Translate,
		Rotate,
		Scale,
	};

	enum class GizmoSpace : u8
	{
		World,
		Local,
	};

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
		ViewportPanel(Renderer& renderer, AppBase& game, Selection& selection);
		~ViewportPanel() override;

		[[nodiscard]] std::string_view GetName() const override { return "Viewport"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_DISPLAY; }
		void OnUpdate() override;
		void OnDraw() override;
		[[nodiscard]] bool HasWindowPadding() const override { return false; }
		[[nodiscard]] bool IsClosable() const override { return false; }

		[[nodiscard]] inline const ViewportTarget* GetTarget() const { return m_valid ? &m_target : nullptr; }
		[[nodiscard]] inline const Camera& GetCamera() const { return m_camera; }
		[[nodiscard]] inline bool IsGameCamera() const { return m_cameraMode == ViewportCamera::Game; }
		[[nodiscard]] inline ViewportCamera GetCameraMode() const { return m_cameraMode; }
		inline void SetCameraMode(ViewportCamera mode) { m_cameraMode = mode; }

		const ViewportTarget* PrepareFrame(CommandList& cmd);
		void FrameBounds(const BoundingBox& bounds);

		// Orbit camera state, persisted per project across editor sessions
		[[nodiscard]] toml::table SaveCameraState() const;
		bool LoadCameraState(const toml::table& table);
		void Update(f32 dt, InputManager& input);

	private:
		void EnsureSize(u32 width, u32 height);
		void DrawToolbar();

		// Image rect in screen space
		void DrawGizmo(f32 x, f32 y, f32 width, f32 height);
		void HandlePicking(f32 x, f32 y, f32 width, f32 height);
		void PickEntity(f32 ndcX, f32 ndcY);

	private:
		Renderer*         m_renderer;
		AppBase*          m_game      = nullptr;
		Selection*        m_selection = nullptr;
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

		GizmoOp           m_gizmoOp    = GizmoOp::Translate;
		GizmoSpace        m_gizmoSpace = GizmoSpace::World;
		bool              m_gizmoActive = false;  // gizmo owns the mouse this frame, camera must not
	};
}
