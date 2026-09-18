#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Cooker.h"
#include "Koi/Editor/EditorApp.h"
#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Editor/Panels/AssetsPanel.h"
#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Panels/LogPanel.h"
#include "Koi/Editor/Panels/RenderTargetsPanel.h"
#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Editor/Panels/WorldOutlinePanel.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"

#include <imgui.h>
#include <fstream>
#include <utility>

namespace koi::editor
{
	EditorApp::EditorApp(std::unique_ptr<AppBase> game, EditorConfig config)
		: m_game(std::move(game))
		, m_config(std::move(config))
	{
		KOI_LOG_DEBUG("Initializing Editor");
		KOI_LOG_DEBUG("\tEngine source path: {}" , m_config.EngineSource.string());
		KOI_LOG_DEBUG("\tProject source path: {}", m_config.ProjectSource.string());
		KOI_LOG_DEBUG("\tProject file path: {}"  , m_config.ProjectFile.string());
		KOI_LOG_DEBUG("\tCook output path: {}"   , m_config.CookedOutput.string());
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

		// Register the renderer as reloadable so that it can recompile shaders and rebuild pipelines
		RegisterReloadable(&renderer);

		// Register the editor context with the game so that it can register panels and reloadables
		m_game->SetEditorContext(this);

		// Scan content and reconcile the manifest before any panel can query it
		m_assetDatabase = std::make_unique<AssetDatabase>(m_config.EngineSource, m_config.ProjectSource);
		m_assetDatabase->Refresh();

		// Game/user project can expect built in inspectors to existat init-time
		RegisterBuiltinInspectors(m_inspectors);

		// Register default editor panels
		m_panels.Emplace<WorldOutlinePanel>(*m_game, m_selection);
		m_panels.Emplace<InspectorPanel>(m_selection, m_inspectors, *m_assetDatabase);
		m_panels.Emplace<RenderTargetsPanel>(renderer);
		m_panels.Emplace<LogPanel>();
		m_viewportPanel     = &m_panels.Emplace<ViewportPanel>(renderer, *m_game, m_selection);
		m_panels.Emplace<AssetsPanel>(*m_assetDatabase, m_selection);
	}

	void EditorApp::HostInit()
	{
		Setup();
		m_game->HostInit();
		m_imgui.Init(Engine::Get().GetWindow());

		if (!LoadEditorState())
		{
			// Failed to load editor state (camera), set editor camera to cover scene bounds
			m_viewportPanel->FrameBounds(m_game->GetWorld().GetBounds());
		}

		KOI_LOG_INFO("Editor initialized");
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
			return m_viewportPanel->LoadCameraState(toml::parse_file(path.string()));
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG_WARN("Editor state '{}' did not parse: {}", path.string(), e.description());
			return false;
		}
	}

	void EditorApp::SaveEditorState() const
	{
		const fs::path path = EditorStatePath();

		toml::table root;
		root.insert("camera", m_viewportPanel->SaveCameraState());

		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);

		std::ofstream out(path, std::ios::trunc);
		if (out)
		{
			out << root;
		}
		else
		{
			KOI_LOG_WARN("Could not write editor state to '{}'", path.string());
		}
	}

	void EditorApp::HostTick(f32 dt)
	{
		ZoneScoped;
		InputManager& input = Engine::Get().GetSubsystem<InputManager>();

		// Requested during the ImGui frame; runs here, before any command list is recording
		if (m_pendingReload)
			ReloadAll(*std::exchange(m_pendingReload, std::nullopt));

		m_viewportPanel->Update(dt, input);

		// The game always ticks. What play state gates is the world's simulation, and
		// World::Update decides that itself, so nothing here has to know the difference
		m_game->HostTick(dt);
	}

	void EditorApp::HostRender()
	{
		ZoneScoped;
		if (CommandList* cmd = Engine::Get().GetSubsystem<Renderer>().GetCommandList())
			m_viewportPanel->PrepareFrame(*cmd);

		m_game->HostRender();

		QueueGizmos();
	}

	void EditorApp::QueueGizmos()
	{
		ZoneScoped;
		if (m_gizmos.SelectionBounds) DrawSelectionBounds();
		if (m_gizmos.Lights)          DrawLightGizmos();
		if (m_gizmos.CameraFrustum)   DrawCameraFrustum();
		if (m_gizmos.ShadowVolume)    DrawShadowVolume();
	}

	void EditorApp::DrawSelectionBounds()
	{
		World& world = m_game->GetWorld();
		// A tag has no storage, so it filters the query rather than appearing as an argument
		world.GetECS().query_builder<const Transform, const MeshRef>().with<Selected>().build()
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
		PassList& passes = Engine::Get().GetSubsystem<Renderer>().GetPasses();

		const ShadowPass* shadow = passes.Find<ShadowPass>();
		if (!shadow || !shadow->GetShadowMap()) return;

		// The ortho volume the caster was fitted to, as the box its matrix maps to clip space
		DebugDraw::Frustum(shadow->GetLightViewProjection(), Color(1.0f, 0.9f, 0.2f, 1.0f));
	}

	void EditorApp::HostShutdown()
	{
		SaveEditorState();
		m_game->HostShutdown();
		m_imgui.Shutdown();
		KOI_LOG_INFO("Editor shut down");
	}

	void EditorApp::HostBeginPlay() { m_game->HostBeginPlay(); }

	void EditorApp::HostEndPlay() { m_game->HostEndPlay(); }

	void EditorApp::RegisterReloadable(IReloadable* reloadable) { m_reloadables.push_back(reloadable); }

	void EditorApp::RegisterPanel(std::unique_ptr<IEditorPanel> panel) { m_panels.Add(std::move(panel)); }

	void EditorApp::RegisterInspector(std::unique_ptr<IComponentInspector> inspector) { m_inspectors.Add(std::move(inspector)); }

	std::optional<RenderView> EditorApp::GetEditorRenderView() const
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

	void EditorApp::ReloadAll(const ReloadRequest& request)
	{
		KOI_LOG_INFO("Hot-reloading {} registered target(s)", m_reloadables.size());
		for (IReloadable* reloadable : m_reloadables)
		{
			reloadable->Reload(request);
		}
	}

	void EditorApp::OnOverlayRender(CommandList& cmd)
	{
		ZoneScoped;
		Engine& engine = Engine::Get();

		const bool gameOwnsKeyboard = engine.IsPlaying() && engine.GetSubsystem<InputManager>().IsGameFocused();
		m_imgui.BeginFrame(gameOwnsKeyboard);

		{
			ZoneScopedN("Panels");
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			DrawMenuBar();
			m_panels.DrawAll();
			PollShortcuts();
		}

		// ScenePass/TonemapPass render into the Viewport panel's offscreen target rather than
		// the swapchain back buffer, so the back buffer needs to be bound/cleared explicitly here
		// before ImGui draws, and the resolved viewport texture needs to be readable by ImGui::Image
		if (const ViewportTarget* vp = m_viewportPanel->GetTarget())
			cmd.TransitionBarrier(vp->ResolvedColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
			m_pendingReload = ReloadRequest{ .Scope = ReloadScope::All };
		else if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal))  //F5: Reload what is changed
			m_pendingReload = ReloadRequest{ .Scope = ReloadScope::Changed };

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))  // Saving
		{
			SaveScene();
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
			m_panels.DrawMenuItems();
			ImGui::Separator();
			DrawGizmoMenu();
			ImGui::Separator();
			m_imgui.DrawThemeMenu();
			ImGui::EndMenu();
		}

		ImGui::Separator();

		const PlayState playState = Engine::Get().GetPlayState();

		if (ImGui::MenuItem(playState == PlayState::Edit ? "Play" : "Stop"))
		{
			if (playState == PlayState::Edit)
				Play();
			else
				Stop();
		}

		if (playState != PlayState::Edit)
		{
			if (ImGui::MenuItem(playState == PlayState::Paused ? "Resume" : "Pause"))
				TogglePause();
		}

		if (!m_lastCookStatus.empty())
		{
			ImGui::Separator();
			ImGui::TextUnformatted(m_lastCookStatus.c_str());
		}

		ImGui::EndMainMenuBar();
	}

	void EditorApp::DrawGizmoMenu()
	{
		if (!ImGui::BeginMenu("Gizmos")) return;

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
			m_lastCookStatus = "Save: world was not loaded from a scene";
			return;
		}

		const bool saved = world.Save();
		m_lastCookStatus = saved ? "Scene saved" : "Scene save FAILED";
	}

	void EditorApp::Cook()
	{
		m_lastCookStatus = editor::Cook(CookRequest
		{
			.EngineSource  = m_config.EngineSource,
			.ProjectSource = m_config.ProjectSource,
			.ProjectFile   = m_config.ProjectFile,
			.OutputRoot    = m_config.CookedOutput,
			.Scenes        = m_game->GetDesc().Scenes,
		});
	}

	void EditorApp::Play()
	{
		ZoneScoped;

		World& world                    = m_game->GetWorld();
		Engine& engine                  = Engine::Get();
		GfxDevice& device               = engine.GetSubsystem<Renderer>().GetDevice();
		AssetRegistry& assets           = engine.GetSubsystem<AssetRegistry>();
		AssetCache<GPUMesh>& meshes     = assets.Cache<GPUMesh>();
		AssetCache<Material>& materials = assets.Cache<Material>();

		m_snapshot = world.SaveToTable();

		// Hold a reference to everything the snapshot names. Stop tears the world down
		// before rebuilding it, and without this the last reference to each asset would
		// drop to zero and the same files would be re-read from disk a moment later
		world.GetECS().each<MeshRef>([&](Entity, MeshRef& mesh)
		{
			const AssetKey key = meshes.GetKey(mesh.Handle);
			if (key) m_pinnedMeshes.push_back(meshes.Acquire(device, key));
		});

		world.GetECS().each<MaterialRef>([&](Entity, MaterialRef& material)
		{
			const AssetKey key = materials.GetKey(material.Handle);
			if (key) m_pinnedMaterials.push_back(materials.Acquire(device, key));
		});

		// Show the game camera by default; the toolbar can still switch back mid-play
		m_cameraBeforePlay = m_viewportPanel->GetCameraMode();
		m_viewportPanel->SetCameraMode(ViewportCamera::Game);

		// Set the state last: it fires OnBeginPlay, and whatever that spawns must land
		// outside the snapshot so Stop discards it
		Engine::Get().SetPlayState(PlayState::Playing);
	}

	void EditorApp::TogglePause()
	{
		ZoneScoped;
		Engine& engine = Engine::Get();
		switch (engine.GetPlayState())
		{
			case PlayState::Playing: engine.SetPlayState(PlayState::Paused);  break;
			case PlayState::Paused:  engine.SetPlayState(PlayState::Playing); break;
			case PlayState::Edit:                                             break;
		}
	}

	void EditorApp::Stop()
	{
		ZoneScoped;

		Engine& engine = Engine::Get();
		if (engine.GetPlayState() == PlayState::Edit) return;

		// Leave play mode first: OnEndPlay still needs the world it ran against
		engine.SetPlayState(PlayState::Edit);

		// The rebuild below recycles entity ids, so a stale selection would resolve to whatever entity happens to land in that slot
		m_selection.Clear();

		World& world = m_game->GetWorld();
		world.Clear();  // Clear all entities, without touching the systems
		world.LoadFromTable(m_snapshot);

		// The rebuilt world took its own references, so the play-time pins can go
		GfxDevice& device     = engine.GetSubsystem<Renderer>().GetDevice();
		AssetRegistry& assets = engine.GetSubsystem<AssetRegistry>();
		for (const MeshHandle handle : m_pinnedMeshes)         assets.Cache<GPUMesh>().Release(device, handle);
		for (const MaterialHandle handle : m_pinnedMaterials)  assets.Cache<Material>().Release(device, handle);

		m_viewportPanel->SetCameraMode(m_cameraBeforePlay);

		m_pinnedMeshes.clear();
		m_pinnedMaterials.clear();
		m_snapshot = toml::table{};
		KOI_LOG_DEBUG("Stop: world restored from snapshot, pinned assets released");
	}
}
