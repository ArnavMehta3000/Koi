#include "Koi/Editor/EditorApp.h"
#include "Koi/Editor/Panels/ScenePanel.h"
#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/ShaderCompiler.h"
#include "Koi/Engine/Graphics/ShaderLoader.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Platform/InputCodes.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Logging/Logger.h"
#include <filesystem>
#include <fstream>
#include <system_error>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_dx12.h>

namespace koi::editor
{
	static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo* /*info*/,
	                          D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
	                          D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
	{
		auto& heap = Engine::Get().GetRenderer().GetDevice().ResourceHeap();
		const DescriptorIndex idx = heap.Allocate();
		*outCpu = heap.CpuHandle(idx);
		*outGpu = heap.GpuHandle(idx);
	}

	static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo* /*info*/,
	                         D3D12_CPU_DESCRIPTOR_HANDLE cpu,
	                         D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
	{
		auto& renderer = Engine::Get().GetRenderer();
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
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

#if KOI_DEBUG
		KOI_LOG_DEBUG("Initializing Editor");
		KOI_LOG_DEBUG("\tEngine source path: {}" , m_config.EngineSource.string());
		KOI_LOG_DEBUG("\tProject source path: {}", m_config.ProjectSource.string());
		KOI_LOG_DEBUG("\tProject file path: {}"  , m_config.ProjectFile.string());
		KOI_LOG_DEBUG("\tCook output path: {}"   , m_config.CookedOutput.string());
#endif
	}

	const AppCreateDesc& EditorApp::GetDesc() const { return m_game->GetDesc(); }

	void EditorApp::SetWindow(Window& window) { m_game->SetWindow(window); }

	void EditorApp::Setup()
	{
		// Hook the editor with the engine/renderer
		Renderer& renderer = Engine::Get().GetRenderer();

		// Set editor as overlay (ImGui)
		renderer.SetOverlay(this);

		// Register the renderer as reloadable so that it can recompile shaders and rebuild pipelines
		RegisterReloadable(&renderer);

		// Register the editor context with the game so that it can register panels and reloadables
		m_game->SetEditorContext(this);

		// Register default editor panels
		RegisterPanel<ScenePanel>(*m_game);
		m_viewportPanel = &RegisterPanel<ViewportPanel>(renderer);
	}

	void EditorApp::HostInit()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		Setup();
		m_game->HostInit();
		InitImGui();

		if (World* world = m_game->GetEditorWorld())
			m_viewportPanel->FrameBounds(world->GetBounds());

		KOI_LOG_INFO("Editor initialized");
	}

	void EditorApp::HostTick(f32 dt)
	{
		m_viewportPanel->Update(dt, Engine::Get().GetInputManager());
		m_game->HostTick(dt);
	}

	void EditorApp::HostRender()
	{
		if (CommandList* cmd = Engine::Get().GetRenderer().GetCommandList())
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

	void EditorApp::InitImGui()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		Engine& engine     = Engine::Get();
		Renderer& renderer = engine.GetRenderer();
		GfxDevice& device  = renderer.GetDevice();

		Window& window = engine.GetWindow();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		ImGui::StyleColorsDark();

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
		KOI_LOG_INFO("ImGui (DX12 + GLFW) initialized");
	}

	void EditorApp::ShutdownImGui()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (!m_imguiInitialized) return;

		Engine::Get().GetRenderer().GetDevice().WaitForIdle();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		m_imguiInitialized = false;
	}

	void EditorApp::RegisterReloadable(IReloadable* reloadable) { m_reloadables.push_back(reloadable); }

	void EditorApp::RegisterPanel(std::unique_ptr<IEditorPanel> panel) { m_panels.push_back(std::move(panel)); }

	std::optional<RenderView> EditorApp::GetEditorRenderView() const
	{
		const ViewportTarget* target = m_viewportPanel->GetTarget();
		if (!target) return std::nullopt;
		return RenderView{ .Camera = &m_viewportPanel->GetCamera(), .Target = *target };
	}

	void EditorApp::ReloadAll()
	{
		for (IReloadable* reloadable : m_reloadables)
		{
			reloadable->Reload();
		}
	}

	void EditorApp::OnOverlayRender(CommandList& cmd)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		Engine& engine     = Engine::Get();
		Renderer& renderer = engine.GetRenderer();
		GfxDevice& device  = renderer.GetDevice();

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Ask engine and game to recompile shaders and rebuild pipelines
		if (engine.GetInputManager().IsKeyPressed(KeyCode::F5))
		{
			// TODO: Add proper hotreloading logic instead of reloading everything
			InvalidateShaderCache();
			ReloadAll();
		}

		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		DrawMenuBar();

		// Draw the panels
		for (auto& panel : m_panels)
		{
			panel->OnDraw();
		}

		// Render ImGui
		ImGui::Render();

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

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

		auto* native = cmd.GetNative();
		ID3D12DescriptorHeap* heaps[] = { device.ResourceHeap().GetHeap() };
		native->SetDescriptorHeaps(1, heaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), native);
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

		if (!m_lastCookStatus.empty())
		{
			ImGui::Separator();
			ImGui::TextUnformatted(m_lastCookStatus.c_str());
		}

		ImGui::EndMainMenuBar();
	}

	void EditorApp::SaveScene()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		World* world = m_game->GetEditorWorld();
		if (!world)
		{
			m_lastCookStatus = "Save: no editor world";
			return;
		}

		const std::string& scenePath = m_game->GetDesc().StartupScene;
		if (scenePath.empty())
		{
			m_lastCookStatus = "Save: no startup_scene set";
			return;
		}

		m_lastCookStatus = world->Save(scenePath) ? "Scene saved" : "Scene save FAILED";
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

		// Compile engine shaders -> <out>/Engine/Shaders/*.cso
		const fs::path engineShaderSrc = m_config.EngineSource / "Shaders";
		const fs::path engineShaderOut = outRoot / "Engine" / "Shaders";
		fs::create_directories(engineShaderOut, ec);

		// Compile project shaders -> <out>/Project/Assets/Shaders/*.cso
		const fs::path projShaderSrc = m_config.ProjectSource / "Assets" / "Shaders";
		const fs::path projShaderOut = projRoot / "Assets" / "Shaders";
		fs::create_directories(projShaderOut, ec);

		u32 shaderOk = 0, shaderTotal = 0;
		auto compileDir = [&](const fs::path& srcDir, const fs::path& outDir, const char* label)
		{
			if (!fs::exists(srcDir)) return;
			for (auto& entry : fs::directory_iterator(srcDir))
			{
				if (!entry.is_regular_file()) continue;
				if (entry.path().extension() != ".hlsl") continue;

				const ShaderType type = InferShaderTypeFromName(entry.path().filename().string());
				if (type == ShaderType::Unknown)
				{
					KOI_LOG_WARN("Cook: skipping '{}' (no VS/PS/CS suffix)", entry.path().filename().string());
					continue;
				}

				++shaderTotal;
				ShaderCompiler::Args args{};
				args.Type   = type;
				args.OutDir = outDir;
				auto blob = ShaderCompiler::Get().CompileFromFile(entry.path(), args);
				if (blob && blob->GetBufferSize() > 0)
				{
					KOI_LOG_INFO("Cook [{}]: {} OK", label, entry.path().filename().string());
					++shaderOk;
				}
				else
				{
					KOI_LOG_ERROR("Cook [{}]: {} FAILED", label, entry.path().filename().string());
				}
			}
		};
		compileDir(engineShaderSrc, engineShaderOut, "engine");
		compileDir(projShaderSrc,   projShaderOut,   "project");

		// non-shader project assets
		auto copyDir = [&](const fs::path& src, const fs::path& dst)
		{
			if (!fs::exists(src)) return;
			std::error_code cec;
			fs::copy(src, dst,
				fs::copy_options::recursive | fs::copy_options::overwrite_existing, cec);
			if (cec)
				KOI_LOG_WARN("Cook: copy {} -> {} failed: {}", src.string(), dst.string(), cec.message());
		};
		copyDir(m_config.ProjectSource / "Assets" / "Models", projRoot / "Assets" / "Models");
		copyDir(m_config.ProjectSource / "Assets" / "Scenes", projRoot / "Assets" / "Scenes");

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
