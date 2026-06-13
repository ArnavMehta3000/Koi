#include "EditorApp.h"
#include "Runtime/Engine.h"
#include "Graphics/Renderer.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/CommandList.h"
#include "Graphics/ShaderCompiler.h"
#include "Graphics/ShaderLoader.h"
#include "Platform/Window.h"
#include "Platform/InputManager.h"
#include "Platform/InputCodes.h"
#include "Memory/MemoryTracker.h"
#include "Core/Logger.h"
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

	EditorApp::EditorApp() = default;

	EditorApp::~EditorApp()
	{
		Shutdown();
	}

	void EditorApp::Initialize(EditorConfig config)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		if (m_initialized) return;
		Engine& engine = Engine::Get();

		m_config = std::move(config);

#if KOI_DEBUG
		KOI_LOG_DEBUG("Initializing Editor");
		KOI_LOG_DEBUG("\tEngine source path: {}" , m_config.EngineSource.string());
		KOI_LOG_DEBUG("\tProject source path: {}", m_config.ProjectSource.string());
		KOI_LOG_DEBUG("\tProject file path: {}"  , m_config.ProjectFile.string());
		KOI_LOG_DEBUG("\tCook output path: {}"   , m_config.CookedOutput.string());
#endif

		// Configure engine hooks
		m_postInitHook    = engine.RegisterHook(EnginePhase::PostInit          , [this] { OnPostInit();   });
		m_postRenderHook  = engine.RegisterHook(EnginePhase::OnSwapchainOverlay, [this] { OnPostRender(); }); 

		m_initialized = true;
		KOI_LOG_INFO("Editor initialized");
	}

	void EditorApp::Shutdown()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (!m_initialized) return;

		Engine& engine = Engine::Get();
		engine.UnregisterHook(m_postInitHook);
		engine.UnregisterHook(m_postRenderHook);
		m_postInitHook   = INVALID_HOOK_ID;
		m_postRenderHook = INVALID_HOOK_ID;

		if (m_imguiInitialized)
		{
			engine.GetRenderer().GetDevice().WaitForIdle();
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
			m_imguiInitialized = false;
		}

		m_initialized = false;
		KOI_LOG_INFO("Editor shut down");
	}

	void EditorApp::OnPostInit()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		Engine& engine     = Engine::Get();
		Renderer& renderer = engine.GetRenderer();
		GfxDevice& device  = renderer.GetDevice();

		WindowHandle mainWnd = engine.GetWindowManager().GetMainWndowHandle();
		Window* window = engine.GetWindowManager().GetWindow(mainWnd);
		if (!window)
		{
			KOI_LOG_ERROR("Editor: main window not available at PostInit");
			return;
		}

		// Init ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForOther(window->GetGLFWWindow(), true);

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

	void EditorApp::OnPostRender()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (!m_imguiInitialized) return;

		Engine& engine         = Engine::Get();
		Renderer& renderer     = engine.GetRenderer();
		GfxDevice& device      = renderer.GetDevice();
		RenderWindowHandle rwh = engine.GetMainRenderWindowHandle();
		CommandList* cmd       = renderer.GetCommandList(rwh);
		if (!cmd) return;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Ask engine and game to recompile shaders and rebuild pipelines
		{
			WindowHandle mainWnd = engine.GetWindowManager().GetMainWndowHandle();
			if (const WindowInput* wi = engine.GetInputManager().GetWindowInput(mainWnd))
			{
				if (wi->IsKeyPressed(KeyCode::F5))
				{
					InvalidateShaderCache();
					engine.FireHook(EnginePhase::OnShaderReload);
				}
			}
		}

		DrawMenuBar();
		ImGui::ShowDemoWindow();

		ImGui::Render();

		auto* native = cmd->GetNative();
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

	void EditorApp::Cook()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		const fs::path outRoot = m_config.CookedOutput;

		KOI_LOG_INFO("Cook: output = {}", outRoot.string());

		std::error_code ec;
		fs::create_directories(outRoot, ec);

		// Cook layout: <out>/Engine/... and <out>/Project/... — clean split so
		// runtime can mount engine:// and project:// at sibling roots.
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
