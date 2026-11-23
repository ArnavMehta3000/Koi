#pragma once
#include "Koi/Editor/Panels/RenderViewport.h"
#include <IconsFontAwesome6.h>
#include <optional>

namespace koi
{
	class AppBase;
}

namespace koi::editor
{
	class CommandRegistry;
	class EditorContext;
	class Selection;
	class UndoStack;

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

	// The game world
	class SceneViewport final : public RenderViewport
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "Viewport", .Icon = ICON_FA_DISPLAY, .Padding = false, .Closable = false };

		explicit SceneViewport(EditorContext* ctx);

		void OnTick(f32 dt) override;

		// Frames the scene when no camera was saved
		void LoadState(const toml::table& state) override;

		[[nodiscard]] bool IsGameCamera() const { return m_cameraMode == ViewportCamera::Game; }

		// What the main SceneRenderer draws this frame: the scene camera, or the game's in game view
		[[nodiscard]] std::optional<RenderView> GetView() const;

	private:
		void DrawToolbar() override;
		void ChooseSize(const ImVec2& avail, u32& width, u32& height) override;
		void OnImageGui(const ImVec2& min, const ImVec2& size) override;
		void OnWindowGui() override;

		void DrawGizmo(const ImVec2& min, const ImVec2& size);
		void HandlePicking(const ImVec2& min, const ImVec2& size);
		void PickEntity(f32 ndcX, f32 ndcY);

	private:
		AppBase*         m_game      = nullptr;
		Selection*       m_selection = nullptr;
		CommandRegistry* m_commands  = nullptr;
		UndoStack*       m_undo      = nullptr;

		ViewportCamera     m_cameraMode       = ViewportCamera::Scene;
		ViewportCamera     m_cameraBeforePlay = ViewportCamera::Scene;  // restored on Stop
		ViewportResolution m_resolution       = ViewportResolution::FreeAspect;

		GizmoOp    m_gizmoOp     = GizmoOp::Translate;
		GizmoSpace m_gizmoSpace  = GizmoSpace::World;
		bool       m_gizmoActive = false;  // gizmo owns the mouse this frame, camera must not
	};
}
