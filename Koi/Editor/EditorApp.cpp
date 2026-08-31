#include "Koi/Editor/EditorApp.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Cooker.h"
#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Editor/Panels/AssetsPanel.h"
#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Panels/LogPanel.h"
#include "Koi/Editor/Panels/RenderTargetsPanel.h"
#include "Koi/Editor/Panels/ScriptEditorPanel.h"
#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Editor/Panels/WorldOutlinePanel.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"

#include <imgui.h>

namespace koi::editor
{
	EditorApp::EditorApp(std::unique_ptr<AppBase> game, EditorConfig config)
		: m_game(std::move(game))
		, m_config(std::move(config))
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

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
		RegisterReloadable(&Engine::Get().GetSubsystem<ScriptEngine>());  // Similarly to above

		// Register the editor context with the game so that it can register panels and reloadables
		m_game->SetEditorContext(this);

		// Scan content and reconcile the manifest before any panel can query it
		m_assetDatabase = std::make_unique<AssetDatabase>(m_config.EngineSource, m_config.ProjectSource);
		m_assetDatabase->Refresh();

		// Game/user project can expect built in inspectors to existat init-time
		RegisterBuiltinInspectors(m_inspectors);

		// Register default editor panels
		m_panels.Emplace<WorldOutlinePanel>(*m_game, m_selection);
		m_panels.Emplace<InspectorPanel>(m_selection, m_inspectors);
		m_panels.Emplace<RenderTargetsPanel>(renderer);
		m_panels.Emplace<LogPanel>();
		m_viewportPanel     = &m_panels.Emplace<ViewportPanel>(renderer, *m_game, m_selection);
		m_scriptEditorPanel = &m_panels.Emplace<ScriptEditorPanel>([this](const ReloadRequest& r) { ReloadAll(r); });
		m_panels.Emplace<AssetsPanel>(*m_assetDatabase, [this](std::string_view vfsPath) { OpenScriptEditor(vfsPath); });
	}

	void EditorApp::HostInit()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		Setup();
		m_game->HostInit();
		m_imgui.Init(Engine::Get().GetWindow());

		m_viewportPanel->FrameBounds(m_game->GetWorld().GetBounds());

		KOI_LOG_INFO("Editor initialized");
	}

	void EditorApp::HostTick(f32 dt)
	{
		ZoneScoped;
		InputManager& input = Engine::Get().GetSubsystem<InputManager>();

		// Hot-reload runs before frame so that commandlist invalidation doesn't happen
		if (input.IsKeyPressed(KeyCode::F5))
		{
			// Shift forces everything; plain F5 leaves each target to reload what changed
			ReloadAll(ReloadRequest
			{
				.Scope = input.IsModifierDown(Modifier::Shift) ? ReloadScope::All : ReloadScope::Changed
			});
		}

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
	}

	void EditorApp::HostShutdown()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
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

	void EditorApp::OpenScriptEditor(std::string_view vfsPath) { m_scriptEditorPanel->Open(vfsPath); }

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
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		m_imgui.BeginFrame();

		{
			ZoneScopedN("Panels");
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			DrawMenuBar();
			m_panels.DrawAll();
		}

		// ScenePass/TonemapPass render into the Viewport panel's offscreen target rather than
		// the swapchain back buffer, so the back buffer needs to be bound/cleared explicitly here
		// before ImGui draws, and the resolved viewport texture needs to be readable by ImGui::Image
		if (const ViewportTarget* vp = m_viewportPanel->GetTarget())
			cmd.TransitionBarrier(vp->ResolvedColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		if (RenderWindow* rw = Engine::Get().GetSubsystem<Renderer>().GetMainRenderWindow())
		{
			static constexpr std::array kClearColor = { 0.05f, 0.05f, 0.05f, 1.0f };
			cmd.TransitionBarrier(rw->CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			cmd.SetRenderTarget(rw->CurrentBackBuffer());
			cmd.ClearRenderTarget(rw->CurrentBackBuffer(), kClearColor.data());
			cmd.SetViewportAndScissor(rw->GetWidth(), rw->GetHeight());
		}

		m_imgui.EndFrame(cmd);
	}

	void EditorApp::DrawMenuBar()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (!ImGui::BeginMainMenuBar()) return;

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Scene"))
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

	void EditorApp::SaveScene()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		const std::string& scenePath = m_game->GetDesc().StartupScene;
		if (scenePath.empty())
		{
			m_lastCookStatus = "Save: no startup_scene set";
			return;
		}

		const bool saved = m_game->GetWorld().Save(scenePath);
		m_lastCookStatus = saved ? "Scene saved" : "Scene save FAILED";
		if (!saved)
		{
			KOI_LOG_ERROR("Failed to save scene '{}'", scenePath);
		}
	}

	void EditorApp::Cook()
	{
		m_lastCookStatus = editor::Cook(CookRequest
		{
			.ProjectSource = m_config.ProjectSource,
			.ProjectFile   = m_config.ProjectFile,
			.OutputRoot    = m_config.CookedOutput,
		}, m_assetDatabase.get());
	}

	void EditorApp::Play()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		World& world                = m_game->GetWorld();
		GfxDevice& device           = Engine::Get().GetSubsystem<Renderer>().GetDevice();
		AssetCache<GPUMesh>& meshes = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();

		m_snapshot = world.SaveToTable();

		// Hold a reference to everything the snapshot names. Stop tears the world down
		// before rebuilding it, and without this the last reference to each mesh would
		// drop to zero and the same files would be re-read from disk a moment later
		world.GetECS().each<MeshRef>([&](Entity, MeshRef& mesh)
		{
			const std::string path(meshes.GetPath(mesh.Handle));
			if (!path.empty()) m_pinnedAssets.push_back(meshes.Acquire(device, path));
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
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

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
		GfxDevice& device           = engine.GetSubsystem<Renderer>().GetDevice();
		AssetCache<GPUMesh>& meshes = engine.GetSubsystem<AssetRegistry>().Cache<GPUMesh>();
		for (const MeshHandle handle : m_pinnedAssets)
		{
			meshes.Release(device, handle);
		}

		m_viewportPanel->SetCameraMode(m_cameraBeforePlay);

		m_pinnedAssets.clear();
		m_snapshot = toml::table{};
		KOI_LOG_DEBUG("Stop: world restored from snapshot, pinned assets released");
	}
}
