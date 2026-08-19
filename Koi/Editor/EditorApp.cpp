#include "Koi/Editor/EditorApp.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Panels/AssetsPanel.h"
#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Panels/LogPanel.h"
#include "Koi/Editor/Panels/RenderTargetsPanel.h"
#include "Koi/Editor/Panels/ScriptEditorPanel.h"
#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Editor/Panels/WorldOutlinePanel.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Editor/ImGui/Themes.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Graphics/ShaderCompiler.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <ImGuizmo.h>

#include <fstream>
#include <system_error>

namespace koi::editor
{
	// Layout for the first run, before ImGui has written an imgui.ini of its own.
	// To change it: arrange the panels in the editor, then copy the resulting ini here
	static constexpr const char* kDefaultLayout = R"ini(
		[Window][WindowOverViewport_11111111]
		Pos=0,22
		Size=2560,1489
		Collapsed=0
		LastUsed=20260819

		[Window][Viewport]
		Pos=0,22
		Size=1839,1059
		Collapsed=0
		DockId=0x00000005,0
		LastUsed=20260819

		[Window][World Outline]
		Pos=1841,22
		Size=719,316
		Collapsed=0
		DockId=0x00000009,0
		LastUsed=20260819

		[Window][Inspector]
		Pos=1841,340
		Size=719,527
		Collapsed=0
		DockId=0x0000000A,0
		LastUsed=20260819

		[Window][Render Targets]
		Pos=1841,869
		Size=719,642
		Collapsed=0
		DockId=0x00000002,0
		LastUsed=20260819

		[Window][Debug##Default]
		Pos=60,60
		Size=400,400
		Collapsed=0
		LastUsed=20260819

		[Window][Log]
		Pos=0,1083
		Size=961,428
		Collapsed=0
		DockId=0x00000007,0
		LastUsed=20260819

		[Window][Text Editor]
		Pos=1841,340
		Size=719,527
		Collapsed=0
		DockId=0x0000000A,1
		LastUsed=20260819

		[Window][Assets]
		Pos=963,1083
		Size=876,428
		Collapsed=0
		DockId=0x00000008,0
		LastUsed=20260819

		[Window][Assets/FolderTree_942ADF46]
		IsChild=1
		Size=190,368
		LastUsed=20260818

		[Table][0x584826A8,4]
		RefScale=16
		Column 0  Width=78 ID=0xDE21DBF3
		Column 1  Width=78 ID=0xFBE609D2
		Column 2  Width=155 ID=0x26563918
		Column 3  Width=612 ID=0xA5F9976D
		LastUsed=20260819

		[Table][0x4FE63A1D,3]
		RefScale=16
		Column 0  Weight=1.0000 ID=0x4637146C
		Column 1  Width=111 ID=0x517A6DEB
		Column 2  Width=298 ID=0x7D73DD88
		LastUsed=20260818

		[Table][0x26345F36,3]
		RefScale=16
		Column 0  Weight=1.0000 ID=0x4637146C
		Column 1  Width=161 ID=0x517A6DEB
		Column 2  Width=154 ID=0x7D73DD88
		LastUsed=20260819

		[Table][0xCC434132,2]
		RefScale=16
		Column 0  Width=228 ID=0x30C046FC
		Column 1  Weight=1.0000 ID=0x24966EC5
		LastUsed=20260819

		[Docking][Data]
		DockSpace       ID=0x08BD597D Window=0x1BBC0F80 Pos=0,51 Size=2560,1489 Split=X
  DockNode      ID=0x00000003 Parent=0x08BD597D SizeRef=1839,701 Split=Y Selected=0xC450F867
    DockNode    ID=0x00000005 Parent=0x00000003 SizeRef=1360,1059 CentralNode=1 Selected=0xC450F867
    DockNode    ID=0x00000006 Parent=0x00000003 SizeRef=1360,428 Split=X Selected=0x139FDA3F
      DockNode  ID=0x00000007 Parent=0x00000006 SizeRef=961,428 Selected=0x139FDA3F
      DockNode  ID=0x00000008 Parent=0x00000006 SizeRef=876,428 Selected=0x42C24103
  DockNode      ID=0x00000004 Parent=0x08BD597D SizeRef=719,701 Split=Y Selected=0xE601B12F
    DockNode    ID=0x00000001 Parent=0x00000004 SizeRef=628,845 Split=Y Selected=0x73D9A15B
      DockNode  ID=0x00000009 Parent=0x00000001 SizeRef=512,316 Selected=0x73D9A15B
      DockNode  ID=0x0000000A Parent=0x00000001 SizeRef=512,527 Selected=0x36DC96AB
    DockNode    ID=0x00000002 Parent=0x00000004 SizeRef=628,642 Selected=0x884BBC23
)ini";

	static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo* /*info*/,
	                          D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
	                          D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
	{
		auto& heap = Engine::Get().GetSubsystem<Renderer>().GetDevice().ResourceHeap();
		const DescriptorIndex idx = heap.Allocate();
		*outCpu = heap.CpuHandle(idx);
		*outGpu = heap.GpuHandle(idx);
	}

	static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo* /*info*/,
	                         D3D12_CPU_DESCRIPTOR_HANDLE cpu,
	                         D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
	{
		auto& renderer = Engine::Get().GetSubsystem<Renderer>();
		auto& device   = renderer.GetDevice();
		auto& heap     = device.ResourceHeap();
		auto* native   = heap.GetHeap();

		const SIZE_T base         = native->GetCPUDescriptorHandleForHeapStart().ptr;
		const SIZE_T stride       = device.GetNative()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		const DescriptorIndex idx = static_cast<DescriptorIndex>((cpu.ptr - base) / stride);
		heap.Free(idx);
	}

	EditorApp::EditorApp(std::unique_ptr<AppBase> game, EditorConfig config)
		: m_game(std::move(game))
		, m_config(std::move(config))
		, m_theme(Theme::CatppuccinMocha)
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

		// Register default editor panels
		RegisterPanel<WorldOutlinePanel>(*m_game, m_selection);
		RegisterPanel<InspectorPanel>(m_selection);
		RegisterPanel<RenderTargetsPanel>(renderer);
		RegisterPanel<LogPanel>();
		m_viewportPanel     = &RegisterPanel<ViewportPanel>(renderer, *m_game, m_selection);
		m_scriptEditorPanel = &RegisterPanel<ScriptEditorPanel>([this](const ReloadRequest& r) { ReloadAll(r); });
		RegisterPanel<AssetsPanel>(*m_assetDatabase, [this](std::string_view vfsPath) { OpenScriptEditor(vfsPath); });
	}

	void EditorApp::HostInit()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		Setup();
		m_game->HostInit();
		InitImGui();

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
		ShutdownImGui();
		KOI_LOG_INFO("Editor shut down");
	}

	void EditorApp::HostBeginPlay() { m_game->HostBeginPlay(); }

	void EditorApp::HostEndPlay() { m_game->HostEndPlay(); }

	void EditorApp::InitImGui()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		Engine& engine     = Engine::Get();
		Renderer& renderer = engine.GetSubsystem<Renderer>();
		GfxDevice& device  = renderer.GetDevice();

		Window& window = engine.GetWindow();

		IMGUI_CHECKVERSION();

		// Route ImGui allocations through operator new so they're tracked by MemoryTracker
		ImGui::SetAllocatorFunctions(
			[](size_t size, void*) -> void*
			{
				KOI_MEM_TAG_SCOPE(MemTag::Editor);
				return ::operator new(size);
			},
			[](void* ptr, void*) { ::operator delete(ptr); });

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		io.ConfigDpiScaleFonts     = true;  // style.FontScaleDpi tracks the viewport's monitor
		io.ConfigDpiScaleViewports = true;  // platform windows scale with it too
		ApplyTheme(m_theme);

		ImGui::GetStyle().FontSizeBase = 13.0f;
		io.Fonts->AddFontDefaultVector();
		// Loading here marks settings as loaded, so ImGui keeps the seeded layout
		// instead of starting empty. Later runs read the user's own ini from disk
		const char* iniPath = io.IniFilename ? io.IniFilename : "imgui.ini";
		if (std::error_code ec; !fs::exists(iniPath, ec))
			ImGui::LoadIniSettingsFromMemory(kDefaultLayout);

		ImGui_ImplGlfw_InitForOther(window.GetGLFWWindow(), true);

		ImGui_ImplDX12_InitInfo initInfo{};
		initInfo.Device               = device.GetNative();
		initInfo.CommandQueue         = device.GetGraphicsQueue();
		initInfo.NumFramesInFlight    = FRAMES_IN_FLIGHT;
		initInfo.RTVFormat            = DXGI_FORMAT_R8G8B8A8_UNORM;  // using swapchain format
		initInfo.SrvDescriptorHeap    = device.ResourceHeap().GetHeap();
		initInfo.SrvDescriptorAllocFn = &ImGuiSrvAlloc;
		initInfo.SrvDescriptorFreeFn  = &ImGuiSrvFree;
		ImGui_ImplDX12_Init(&initInfo);

		m_imguiInitialized = true;
		KOI_LOG_DEBUG("ImGui (DX12 + GLFW) initialized");
	}

	void EditorApp::ShutdownImGui()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (!m_imguiInitialized) return;

		Engine::Get().GetSubsystem<Renderer>().GetDevice().WaitForIdle();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		m_imguiInitialized = false;

		KOI_LOG_DEBUG("ImGui shut down");
	}

	void EditorApp::RegisterReloadable(IReloadable* reloadable) { m_reloadables.push_back(reloadable); }

	void EditorApp::RegisterPanel(std::unique_ptr<IEditorPanel> panel)
	{
		KOI_LOG_DEBUG("Registered editor panel '{}'", panel->GetName());
		m_panels.push_back(std::move(panel));
		m_panelOpen.push_back(true);
	}

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

		Engine& engine     = Engine::Get();
		Renderer& renderer = engine.GetSubsystem<Renderer>();
		GfxDevice& device  = renderer.GetDevice();

		// Scope-tracked ImGui new frame
		{
			ZoneScopedN("ImGui NewFrame");
			{ ZoneScopedN("Backend DX12"); ImGui_ImplDX12_NewFrame(); }
			{ ZoneScopedN("Backend GLFW"); ImGui_ImplGlfw_NewFrame(); }
			{ ZoneScopedN("Core");         ImGui::NewFrame();         }
			{ ZoneScopedN("ImGuizmo");     ImGuizmo::BeginFrame();    }
		}

		{
			ZoneScopedN("Panels");
			ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			DrawMenuBar();

			for (size_t i = 0; i < m_panels.size(); ++i)
			{
				if (!m_panelOpen[i]) continue;

				IEditorPanel& panel = *m_panels[i];
				ZoneTransientN(panelZone, panel.GetName().data(), true);  // every GetName() is a literal
				panel.OnDraw();
			}
		}

		{
			ZoneScopedN("ImGui Render");
			ImGui::Render();

			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		// ScenePass/TonemapPass render into the Viewport panel's offscreen target rather than
		// the swapchain back buffer, so the back buffer needs to be bound/cleared explicitly here
		// before ImGui draws, and the resolved viewport texture needs to be readable by ImGui::Image
		if (const ViewportTarget* vp = m_viewportPanel->GetTarget())
			cmd.TransitionBarrier(vp->ResolvedColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		if (RenderWindow* rw = renderer.GetMainRenderWindow())
		{
			static constexpr std::array kClearColor = { 0.05f, 0.05f, 0.05f, 1.0f };
			cmd.TransitionBarrier(rw->CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			cmd.SetRenderTarget(rw->CurrentBackBuffer());
			cmd.ClearRenderTarget(rw->CurrentBackBuffer(), kClearColor.data());
			cmd.SetViewportAndScissor(rw->GetWidth(), rw->GetHeight());
		}

		{
			ZoneScopedN("ImGui RecordDrawData");
			auto* native = cmd.GetNative();
			ID3D12DescriptorHeap* heaps[] = { device.ResourceHeap().GetHeap() };
			native->SetDescriptorHeaps(1, heaps);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), native);
		}
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
			for (size_t i = 0; i < m_panels.size(); ++i)
			{
				const IEditorPanel& panel = *m_panels[i];
				if (!panel.IsClosable()) continue;

				bool open = m_panelOpen[i];
				if (ImGui::MenuItem(panel.GetName().data(), nullptr, open))
					m_panelOpen[i] = !open;
			}

			ImGui::Separator();

			if (ImGui::BeginMenu("Theme"))
			{
				for (size_t i = 0; i < kThemes.size(); ++i)
				{
					const Theme theme = static_cast<Theme>(i);
					if (ImGui::MenuItem(kThemes[i].Name, nullptr, theme == m_theme))
					{
						m_theme = theme;
						ApplyTheme(m_theme);
					}
				}
				ImGui::EndMenu();
			}
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
		KOI_LOG_DEBUG("Play: pinned {} asset(s) across the snapshot", m_pinnedAssets.size());
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

		GfxDevice& device           = engine.GetSubsystem<Renderer>().GetDevice();
		AssetCache<GPUMesh>& meshes = engine.GetSubsystem<AssetRegistry>().Cache<GPUMesh>();

		World& world = m_game->GetWorld();
		world.Clear(device);
		world.LoadFromTable(m_snapshot, device);

		// The rebuilt world took its own references, so the play-time pins can go
		for (const MeshHandle handle : m_pinnedAssets)
		{
			meshes.Release(device, handle);
		}

		m_viewportPanel->SetCameraMode(m_cameraBeforePlay);

		m_pinnedAssets.clear();
		m_snapshot = toml::table{};
		KOI_LOG_DEBUG("Stop: world restored from snapshot, pinned assets released");
	}

	void EditorApp::Cook()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		const fs::path outRoot = m_config.CookedOutput;

		KOI_LOG_INFO("Cook: output = {}", outRoot.string());

		std::error_code ec;
		fs::create_directories(outRoot, ec);

		// Cook layout: <out>/Engine/... and <out>/Project/
		const fs::path projRoot = outRoot / "Project";

		// Cook exactly the shaders the registered pipelines use, mirroring the virtual path
		// layout, since that is what the runtime VFS resolves against
		const std::vector<std::string> shaderPaths =
			Engine::Get().GetSubsystem<Renderer>().GetPipelineLibrary().CollectShaderPaths();

		u32 shaderOk = 0, shaderTotal = 0;
		for (const std::string& virtualPath : shaderPaths)
		{
			++shaderTotal;

			const auto split = VirtualFileSystem::SplitVirtualPath(virtualPath);
			if (!split)
			{
				KOI_LOG_ERROR("Cook: '{}' is not a virtual path", virtualPath);
				continue;
			}

			const auto [scheme, relative] = *split;
			if (scheme != "engine" && scheme != "project")
			{
				KOI_LOG_ERROR("Cook: unknown scheme '{}://' on '{}'", scheme, virtualPath);
				continue;
			}

			const ShaderType type = InferShaderTypeFromName(virtualPath);
			if (type == ShaderType::Unknown)
			{
				KOI_LOG_ERROR("Cook: cannot infer stage from '{}'", virtualPath);
				continue;
			}

			const auto source = VirtualFileSystem::Get().TryResolveToDisk(std::string(virtualPath) + ".hlsl");
			if (!source || !fs::exists(*source))
			{
				KOI_LOG_ERROR("Cook: source for '{}' not found on disk", virtualPath);
				continue;
			}

			const fs::path outDir = outRoot
				/ (scheme == "engine" ? "Engine" : "Project")
				/ fs::path(relative).parent_path();
			fs::create_directories(outDir, ec);

			ShaderCompiler::Args args{ .Type = type, .OutDir = outDir };
			auto blob = ShaderCompiler::Get().CompileFromFile(*source, args);
			if (blob && blob->GetBufferSize() > 0)
			{
				KOI_LOG_DEBUG("Cook: {} OK", virtualPath);
				++shaderOk;
			}
			else
			{
				KOI_LOG_ERROR("Cook: {} FAILED", virtualPath);
			}
		}

		// non-shader project assets
		auto copyDir = [&](const fs::path& src, const fs::path& dst)
		{
			if (!fs::exists(src)) return;
			std::error_code cec;

			// A recursive directory copy needs the destination's parent to already exist
			fs::create_directories(dst, cec);
			fs::copy(src, dst,
				fs::copy_options::recursive | fs::copy_options::overwrite_existing, cec);
			if (cec)
				KOI_LOG_WARN("Cook: copy {} -> {} failed: {}", src.string(), dst.string(), cec.message());
			else
				KOI_LOG_DEBUG("Cook: copied {} -> {}", src.string(), dst.string());
		};
		copyDir(m_config.ProjectSource / "Assets" / "Models", projRoot / "Assets" / "Models");
		copyDir(m_config.ProjectSource / "Assets" / "Scenes", projRoot / "Assets" / "Scenes");
		copyDir(m_config.ProjectSource / "Assets" / "Scripts", projRoot / "Assets" / "Scripts");

		// Identity manifest: what lets a cooked build resolve the uuids a scene references
		if (m_assetDatabase)
		{
			fs::create_directories(projRoot, ec);
			m_assetDatabase->SaveIdentityManifest(projRoot / "AssetRegistry.toml");
		}

		// Copy the project TOML as <out>/Project/Project.toml
		if (fs::exists(m_config.ProjectFile))
		{
			fs::create_directories(projRoot, ec);
			std::error_code cec;
			fs::copy_file(m_config.ProjectFile, projRoot / "Project.toml",
				fs::copy_options::overwrite_existing, cec);
			if (cec)
				KOI_LOG_WARN("Cook: copy project TOML failed: {}", cec.message());
		}

		m_lastCookStatus = "Cook: " + std::to_string(shaderOk) + "/" + std::to_string(shaderTotal) + " shaders ok";
		KOI_LOG_INFO("{}", m_lastCookStatus);
	}
}
