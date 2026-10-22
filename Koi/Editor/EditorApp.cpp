#include "Koi/Editor/EditorApp.h"

#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Assets/ImportQueue.h"
#include "Koi/Editor/Cooker.h"
#include "Koi/Editor/ImGui/EditorPalette.h"
#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Panels/AssetsPanel.h"
#include "Koi/Editor/Panels/GridPanel.h"
#include "Koi/Editor/Panels/ImportPanel.h"
#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Panels/LogPanel.h"
#include "Koi/Editor/Panels/PanelHost.h"
#include "Koi/Editor/Panels/ProfilerPanel.h"
#include "Koi/Editor/Panels/RenderTargetsPanel.h"
#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Editor/Panels/WorldOutlinePanel.h"
#include "Koi/Editor/PlaySession.h"
#include "Koi/Editor/Reloader.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Editor/Toasts.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderPasses/GridPass.h"
#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include "Koi/Engine/Runtime/Serializer.h"

#include <fstream>
#include <IconsFontAwesome6.h>
#include <imgui_internal.h>

namespace koi::editor
{
	EditorApp::EditorApp(std::unique_ptr<AppBase> game, EditorConfig config)
		: m_game(std::move(game))
		, m_config(std::move(config))
		, m_context(m_game.get())
	{
		KOI_LOG(Editor, Debug, "Initializing Editor");
		KOI_LOG(Editor, Debug, "\tEngine source path: {}" , m_config.EngineSource.string());
		KOI_LOG(Editor, Debug, "\tProject source path: {}", m_config.ProjectSource.string());
		KOI_LOG(Editor, Debug, "\tProject file path: {}"  , m_config.ProjectFile.string());
		KOI_LOG(Editor, Debug, "\tCook output path: {}"   , m_config.CookedOutput.string());
	}

	const AppCreateDesc& EditorApp::GetDesc() const { return m_game->GetDesc(); }

	EditorApp::~EditorApp() = default;

	void EditorApp::SetWindow(Window& window) { m_game->SetWindow(window); }

	void EditorApp::Setup()
	{
		// Hook the editor with the engine/renderer
		Renderer& renderer = Engine::Get().GetSubsystem<Renderer>();

		// Set editor as overlay (ImGui)
		renderer.SetOverlay(this);

		// Services first; panels fetch them as they're constructed, so order matters
		m_context.Add<Selection>();
		m_toasts   = m_context.Add<Toasts>();
		m_reloader = m_context.Add<Reloader>();
		m_reloader->Register(&renderer);  // recompiles shaders and rebuilds pipelines

		// Scan content and reconcile the manifest before any panel can query it
		AssetDatabase* database = m_context.Add<AssetDatabase>(m_config.EngineSource, m_config.ProjectSource);
		database->Refresh();
		m_importQueue = m_context.Add<ImportQueue>(*database);

		// Extensions can expect the built-in inspectors to exist at startup
		m_context.Add<InspectorRegistry>()->RegisterBuiltins();
		m_play = m_context.Add<PlaySession>();

		m_panels = m_context.Add<PanelHost>();
		m_panels->Emplace<WorldOutlinePanel>();
		m_panels->Emplace<InspectorPanel>();
		m_panels->Emplace<RenderTargetsPanel>();
		m_panels->Emplace<GridPanel>();
		m_panels->Emplace<LogPanel>();
		m_panels->Emplace<ProfilerPanel>();
		m_viewportPanel = m_panels->Emplace<ViewportPanel>();
		m_panels->Emplace<AssetsPanel>();
		m_importPanel = m_panels->Emplace<ImportPanel>();

		// Files dragged in from Explorer land in the Import panel to have their settings checked
		Engine::Get().GetWindow().SetDropCallback([this](std::span<const fs::path> files) { m_importQueue->AddFiles(files); });
	}

	void EditorApp::HostInit()
	{
		Setup();
		m_game->HostInit();
		StartExtensions();
		m_imgui.Init(Engine::Get().GetWindow());
		m_panels->RegisterSettingsHandler();

		if (!LoadEditorState())
		{
			// Failed to load editor state (camera), set editor camera to cover scene bounds
			m_viewportPanel->FrameBounds(m_game->GetWorld().GetBounds());
		}

		KOI_LOG(Editor, Info, "Editor initialized");
	}

	void EditorApp::StartExtensions()
	{
		for (const EditorExtensionRegistry::Entry& entry : EditorExtensionRegistry::All())
		{
			KOI_LOG(Editor, Debug, "Starting editor extension '{}'", entry.Name);
			m_extensions.push_back(entry.Create());
			m_extensions.back()->OnStartup(m_context);
		}
	}

	void EditorApp::ShutdownExtensions()
	{
		for (auto it = m_extensions.rbegin(); it != m_extensions.rend(); ++it)
		{
			(*it)->OnShutdown();
		}
		m_extensions.clear();
	}

	fs::path EditorApp::EditorStatePath() const
	{
		return m_config.CookedOutput.parent_path() / "Editor.toml";  // <project>/.koi/Editor.toml
	}

	bool EditorApp::LoadEditorState()
	{
		const fs::path path = EditorStatePath();
		if (!fs::exists(path)) return false;

		try
		{
			const toml::table root = toml::parse_file(path.string());

			if (const toml::node* grid = root.get(GridPass::Settings::kKey))
			{
				if (GridPass* pass = Engine::Get().GetSubsystem<Renderer>().GetFeatures().Find<GridPass>())
				{
					if (auto settings = Serializer<GridPass::Settings>::Deserialize(*grid))
						pass->GetSettings() = *settings;
				}
			}

			return m_viewportPanel->LoadCameraState(root);
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG(Editor, Warn, "Editor state '{}' did not parse: {}", path.string(), e.description());
			return false;
		}
	}

	void EditorApp::SaveEditorState() const
	{
		const fs::path path = EditorStatePath();

		toml::table root;
		root.insert("camera", m_viewportPanel->SaveCameraState());

		if (const GridPass* pass = Engine::Get().GetSubsystem<Renderer>().GetFeatures().Find<GridPass>())
			root.insert(GridPass::Settings::kKey, Serializer<GridPass::Settings>::Serialize(pass->GetSettings()));

		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);

		std::ofstream out(path, std::ios::trunc);
		if (out)
		{
			out << koi::utils::TomlDocument(root);
		}
		else
		{
			KOI_LOG(Editor, Warn, "Could not write editor state to '{}'", path.string());
		}
	}

	void EditorApp::HostTick(f32 dt)
	{
		ZoneScoped;
		InputManager& input = Engine::Get().GetSubsystem<InputManager>();

		// Requested during the ImGui frame; runs here, before any command list is recording
		m_reloader->Flush();

		m_viewportPanel->Update(dt, input);
		m_importQueue->Update();
		if (m_importQueue->ConsumeAttention()) m_importPanel->Focus();

		// The game always ticks. What play state gates is the world's simulation, and
		// World::Update decides that itself, so nothing here has to know the difference
		try
		{
			m_game->HostTick(dt);
		}
		catch (const std::exception& e)
		{
			if (Engine::Get().GetPlayState() == PlayState::Edit) throw;
			m_play->ReportFailure(e);
			m_play->Stop();
		}
	}

	void EditorApp::HostRender()
	{
		ZoneScoped;
		m_viewportPanel->PrepareFrame();

		m_game->HostRenderInto(GetViewportView());

		QueueGizmos();
	}

	void EditorApp::QueueGizmos()
	{
		KOI_SCOPE("Editor Gizmos");
		if (m_gizmos.SelectionBounds) DrawSelectionBounds();
		if (m_gizmos.Lights)          DrawLightGizmos();
		if (m_gizmos.CameraFrustum)   DrawCameraFrustum();
		if (m_gizmos.ShadowVolume)    DrawShadowVolume();
	}

	void EditorApp::DrawSelectionBounds()
	{
		World& world = m_game->GetWorld();
		// A tag has no storage, so it filters the query rather than appearing as an argument
		world.GetECS()
			.query_builder<const Transform, const MeshRef>()
			.with<Selected>()
			.build()
			.each([&](const Transform& xform, const MeshRef& meshRef)
			{
				const GPUMesh* mesh = world.GetMesh(meshRef.Handle);
				if (!mesh) return;

				BoundingOrientedBox box;
				BoundingOrientedBox::CreateFromBoundingBox(box, mesh->Bounds);
				box.Transform(box, xform.ToMatrix());

				DebugDraw::Box(box, Color(1.0f, 0.6f, 0.1f, 1.0f));
			});
	}

	void EditorApp::DrawLightGizmos()
	{
		m_game->GetWorld().GetECS().each([](const Transform& xform, const Light& light)
		{
			const Color color(light.Color.x, light.Color.y, light.Color.z, 1.0f);

			if (light.Type == LightType::Point)
			{
				DebugDraw::Sphere(xform.Position, light.Range, color);
				return;
			}

			// Directional: a short shaft along the entity's forward says which way it points
			Vector3 forward = Matrix::CreateFromQuaternion(xform.Rotation).Forward();
			forward.Normalize();

			DebugDraw::Sphere(xform.Position, 0.25f, color);
			DebugDraw::Line(xform.Position, xform.Position + forward * 2.0f, color);
		});
	}

	void EditorApp::DrawCameraFrustum()
	{
		// Pointless while the viewport is already looking through it
		if (m_viewportPanel->IsGameCamera()) return;

		const ViewportTarget* target = m_viewportPanel->GetTarget();
		if (!target || target->Height == 0) return;

		const f32 aspect = static_cast<f32>(target->Width) / static_cast<f32>(target->Height);
		if (const Camera* camera = m_game->GetWorld().ResolveCamera(aspect))
			DebugDraw::Frustum(camera->ViewProjection, Color(0.2f, 0.8f, 1.0f, 1.0f));
	}

	void EditorApp::DrawShadowVolume()
	{
		FeatureList& features = Engine::Get().GetSubsystem<Renderer>().GetFeatures();

		const ShadowPass* shadow = features.Find<ShadowPass>();
		if (!shadow || !shadow->HasShadow()) return;

		// The ortho volume the caster was fitted to, as the box its matrix maps to clip space
		DebugDraw::Frustum(shadow->GetLightViewProjection(), Color(1.0f, 0.9f, 0.2f, 1.0f));
	}

	void EditorApp::HostShutdown()
	{
		Engine::Get().GetWindow().SetDropCallback({});
		SaveEditorState();
		ShutdownExtensions();
		m_context.Shutdown();  // cancels running imports and waits for them
		m_game->HostShutdown();
		m_imgui.Shutdown();
		KOI_LOG(Editor, Info, "Editor shut down");
	}

	void EditorApp::HostBeginPlay()
	{
		try
		{
			m_game->HostBeginPlay();
		}
		catch (const std::exception& e)
		{
			m_play->ReportFailure(e);
			m_play->Stop();
		}
	}

	// Already on the way out of play, so there's nothing left to stop
	void EditorApp::HostEndPlay()
	{
		try
		{
			m_game->HostEndPlay();
		}
		catch (const std::exception& e)
		{
			m_play->ReportFailure(e);
		}
	}

	std::optional<RenderView> EditorApp::GetViewportView() const
	{
		const ViewportTarget* target = m_viewportPanel->GetTarget();
		if (!target || target->Height == 0) return std::nullopt;

		// Game view swaps the camera but keeps the same viewport texture. The scene
		// camera tracks the panel's aspect itself, so only the game one is projected here
		const Camera* camera = &m_viewportPanel->GetCamera();
		if (m_viewportPanel->IsGameCamera())
		{
			const f32 aspect = static_cast<f32>(target->Width) / static_cast<f32>(target->Height);

			// No primary camera in the world yet
			if (const Camera* gameCamera = m_game->GetWorld().ResolveCamera(aspect))
				camera = gameCamera;
		}

		return RenderView{ .Camera = camera, .Target = *target };
	}

	void EditorApp::OnOverlayRender(CommandList& cmd)
	{
		ZoneScoped;
		Engine& engine = Engine::Get();

		const bool gameOwnsKeyboard = engine.IsPlaying() && engine.GetSubsystem<InputManager>().IsGameFocused();
		m_imgui.SetPlayState(engine.GetPlayState());
		m_imgui.BeginFrame(gameOwnsKeyboard);

		{
			KOI_SCOPE("ImGui Panels");
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			DrawMenuBar();
			m_statusBar.Draw({
				.State     = engine.GetPlayState(),
				.ScenePath = m_game->GetWorld().GetSourcePath(),
				.Importing = m_importQueue->IsBusy(),
			});
			m_panels->DrawAll();
			m_toasts->Draw();
			PollShortcuts();
		}

		// Panels above asked for the targets they sample; satisfy all of them in one batch before ImGui::Image reads them
		engine.GetSubsystem<Renderer>().FlushShaderReadRequests(cmd);

		// The pipeline renders into the Viewport panel's offscreen target rather than
		// the swapchain back buffer, so the back buffer needs to be bound/cleared explicitly here
		if (RenderWindow* rw = engine.GetSubsystem<Renderer>().GetMainRenderWindow())
		{
			static constexpr std::array kClearColor = { 0.05f, 0.05f, 0.05f, 1.0f };
			cmd.TransitionBarrier(rw->CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			cmd.SetRenderTarget(rw->CurrentBackBuffer());
			cmd.ClearRenderTarget(rw->CurrentBackBuffer(), kClearColor.data());
			cmd.SetViewportAndScissor(rw->GetWidth(), rw->GetHeight());
		}

		m_imgui.EndFrame(cmd);
	}

	// Global shortcuts go through ImGui's routing
	void EditorApp::PollShortcuts()
	{
		if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_F5, ImGuiInputFlags_RouteGlobal))  // Shift+F5: reload everything
			m_reloader->Request({ .Scope = ReloadScope::All });
		else if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal))  //F5: Reload what is changed
			m_reloader->Request({ .Scope = ReloadScope::Changed });

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))  // Saving
		{
			SaveScene();
		}

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P, ImGuiInputFlags_RouteGlobal))
		{
			m_play->TogglePause();
		}
		else if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, ImGuiInputFlags_RouteGlobal))
		{
			m_play->TogglePlay();
		}
	}

	void EditorApp::DrawMenuBar()
	{
		if (!ImGui::BeginMainMenuBar()) return;

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
				SaveScene();
			if (ImGui::MenuItem("Cook"))
				Cook();
			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::BeginMenu("View"))
		{
			m_panels->DrawMenuItems();
			ImGui::Separator();
			DrawGizmoMenu();
			ImGui::Separator();
			m_imgui.DrawAppearanceMenu();
			ImGui::EndMenu();
		}

		DrawPlayControls();
		ImGui::EndMainMenuBar();
	}

	void EditorApp::DrawPlayControls()
	{
		const PlayState state = Engine::Get().GetPlayState();
		const EditorPalette& palette = EditorPalette::Get();

		const f32 width = ImGui::GetFrameHeight() * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		ImGui::SetCursorPosX(ImMax(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - width) * 0.5f));

		if (utils::IconButton(ICON_FA_PLAY, "Play (Ctrl+P)", state == PlayState::Playing, palette.Playing))
		{
			if (state == PlayState::Edit)
				m_play->Play();
			else if (state == PlayState::Paused)
				m_play->TogglePause();
		}

		ImGui::BeginDisabled(state == PlayState::Edit);
		if (utils::IconButton(ICON_FA_PAUSE, "Pause (Ctrl+Shift+P)", state == PlayState::Paused, palette.Paused))
		{
			m_play->TogglePause();
		}
		if (utils::IconButton(ICON_FA_STOP, "Stop (Ctrl+P)"))
		{
			m_play->Stop();
		}
		ImGui::EndDisabled();

		// Amber rule under the menu bar while the game runs
		if (state != PlayState::Edit)
		{
			const ImRect bar = ImGui::GetCurrentWindow()->Rect();
			ImGui::GetForegroundDrawList(ImGui::GetWindowViewport())->AddLine(ImVec2(bar.Min.x, bar.Max.y - 1.0f), ImVec2(bar.Max.x, bar.Max.y - 1.0f),
				ImGui::GetColorU32(state == PlayState::Playing ? palette.Playing : palette.Paused), 2.0f);
		}
	}

	void EditorApp::DrawGizmoMenu()
	{
		if (!ImGui::BeginMenu("Gizmos")) return;

		// The grid lives in a pass, so its own Settings is the toggle; see the Grid panel
		if (GridPass* grid = Engine::Get().GetSubsystem<Renderer>().GetFeatures().Find<GridPass>())
			ImGui::MenuItem("Grid", nullptr, &grid->GetSettings().Enabled);

		ImGui::MenuItem("Selection Bounds", nullptr, &m_gizmos.SelectionBounds);
		ImGui::MenuItem("Lights",           nullptr, &m_gizmos.Lights);
		ImGui::MenuItem("Camera Frustum",   nullptr, &m_gizmos.CameraFrustum);
		ImGui::MenuItem("Shadow Volume",    nullptr, &m_gizmos.ShadowVolume);

		ImGui::EndMenu();
	}

	void EditorApp::SaveScene()
	{
		ZoneScoped;

		World& world = m_game->GetWorld();
		if (world.GetSourcePath().empty())
		{
			m_toasts->Push(ToastKind::Warning, "Nothing to save: the world was not loaded from a scene");
			return;
		}

		if (const Result<void> saved = world.Save())
		{
			m_toasts->Push(ToastKind::Success, "Scene saved");
		}
		else
		{
			KOI_LOG(Editor, Error, "Scene save failed: {}", saved.error().Message);
			m_toasts->Push(ToastKind::Error, "Scene save failed: " + saved.error().Message);
		}
	}

	void EditorApp::Cook()
	{
		m_toasts->Push(ToastKind::Info, editor::Cook(CookRequest
		{
			.EngineSource  = m_config.EngineSource,
			.ProjectSource = m_config.ProjectSource,
			.ProjectFile   = m_config.ProjectFile,
			.OutputRoot    = m_config.CookedOutput,
			.Scenes        = m_game->GetDesc().Scenes,
		}));
	}
}
