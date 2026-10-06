#include "Koi/Editor/ImGui/ImGuiLayer.h"
#include "Koi/Editor/ImGui/Themes.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/Engine.h"

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <ImGuizmo.h>

#include <filesystem>
#include <string_view>
#include <system_error>

// Koi/Editor/Content files, linked in as objects by xmake's utils.bin2obj rule
extern "C" const char _binary_DefaultLayout_ini_start[];
extern "C" const char _binary_DefaultLayout_ini_end[];
extern "C" const char _binary_MapleMonoRegular_ttf_start[];
extern "C" const char _binary_MapleMonoRegular_ttf_end[];
extern "C" const char _binary_FontAwesomeSolid_ttf_start[];
extern "C" const char _binary_FontAwesomeSolid_ttf_end[];

namespace koi::editor
{
	namespace
	{
		static std::string_view DefaultLayout()
		{
			return std::string_view(_binary_DefaultLayout_ini_start,
				static_cast<size_t>(_binary_DefaultLayout_ini_end - _binary_DefaultLayout_ini_start));
		}

		// The atlas reads the embedded data for its whole lifetime, so it must not take ownership
		static void AddEmbeddedFont(const char* start, const char* end, f32 sizePixels, ImFontConfig config)
		{
			config.FontDataOwnedByAtlas = false;
			ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<char*>(start), static_cast<int>(end - start), sizePixels, &config);
		}

		static void LoadFonts(f32 sizePixels)
		{
			AddEmbeddedFont(_binary_MapleMonoRegular_ttf_start, _binary_MapleMonoRegular_ttf_end, sizePixels, {});

			ImFontConfig icons;
			icons.MergeMode        = true;
			icons.GlyphMinAdvanceX = sizePixels;  // fixed-width icons line up in lists
			AddEmbeddedFont(_binary_FontAwesomeSolid_ttf_start, _binary_FontAwesomeSolid_ttf_end, sizePixels * 0.85f, icons);
		}

		static void SrvAlloc(ImGui_ImplDX12_InitInfo* /*info*/,
							 D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
							 D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
		{
			auto& heap = Engine::Get().GetSubsystem<Renderer>().GetDevice().ResourceHeap();
			const DescriptorIndex idx = heap.Allocate();
			*outCpu = heap.CpuHandle(idx);
			*outGpu = heap.GpuHandle(idx);
		}

		static void SrvFree(ImGui_ImplDX12_InitInfo* /*info*/,
							D3D12_CPU_DESCRIPTOR_HANDLE cpu,
							D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
		{
			auto& device = Engine::Get().GetSubsystem<Renderer>().GetDevice();
			auto& heap   = device.ResourceHeap();
			auto* native = heap.GetHeap();

			const SIZE_T base         = native->GetCPUDescriptorHandleForHeapStart().ptr;
			const SIZE_T stride       = device.GetNative()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			const DescriptorIndex idx = static_cast<DescriptorIndex>((cpu.ptr - base) / stride);
			heap.Free(idx);
		}
	}

	ImGuiLayer::~ImGuiLayer() { Shutdown(); }

	void ImGuiLayer::Init(Window& window)
	{
		GfxDevice& device = Engine::Get().GetSubsystem<Renderer>().GetDevice();

		IMGUI_CHECKVERSION();

		// Route ImGui allocations through operator new so Tracy sees them in profile builds
		ImGui::SetAllocatorFunctions(
			[](size_t size, void*) -> void*
			{
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

		m_theme    = Theme::Zero;
		m_dpiScale = window.GetContentScale();
		ImGui::GetStyle().FontScaleDpi = m_dpiScale;
		utils::ApplyTheme(m_theme, m_dpiScale);

		constexpr f32 kFontSize = 14.0f;
		ImGui::GetStyle().FontSizeBase = kFontSize;
		LoadFonts(kFontSize);

		// If there is no local layout file, load default from memory
		const char* iniPath = io.IniFilename ? io.IniFilename : "imgui.ini";
		if (std::error_code ec; !std::filesystem::exists(iniPath, ec))
		{
			const std::string_view layout = DefaultLayout();
			ImGui::LoadIniSettingsFromMemory(layout.data(), layout.size());
		}

		ImGui_ImplGlfw_InitForOther(window.GetGLFWWindow(), true);

		ImGui_ImplDX12_InitInfo initInfo{};
		initInfo.Device               = device.GetNative();
		initInfo.CommandQueue         = device.GetGraphicsQueue();
		initInfo.NumFramesInFlight    = FRAMES_IN_FLIGHT;
		initInfo.RTVFormat            = DXGI_FORMAT_R8G8B8A8_UNORM;  // using swapchain format
		initInfo.SrvDescriptorHeap    = device.ResourceHeap().GetHeap();
		initInfo.SrvDescriptorAllocFn = &SrvAlloc;
		initInfo.SrvDescriptorFreeFn  = &SrvFree;
		ImGui_ImplDX12_Init(&initInfo);

		m_initialized = true;
		KOI_LOG_DEBUG("ImGui (DX12 + GLFW) initialized");
	}

	void ImGuiLayer::Shutdown()
	{
		if (!m_initialized) return;

		Engine::Get().GetSubsystem<Renderer>().GetDevice().WaitForIdle();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		m_initialized = false;

		KOI_LOG_DEBUG("ImGui shut down");
	}

	void ImGuiLayer::BeginFrame(bool gameOwnsKeyboard)
	{
		KOI_SCOPE("ImGui NewFrame");

		ImGuiIO& io = ImGui::GetIO();
		if (gameOwnsKeyboard)
		{
			io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
			io.ClearInputKeys();
			ImGui::SetNextFrameWantCaptureKeyboard(false);
		}
		else
		{
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		}

		{ ZoneScopedN("Backend DX12"); ImGui_ImplDX12_NewFrame(); }
		{ ZoneScopedN("Backend GLFW"); ImGui_ImplGlfw_NewFrame(); }
		{ ZoneScopedN("Core");         ImGui::NewFrame();         }
		{ ZoneScopedN("ImGuizmo");     ImGuizmo::BeginFrame();    }
	}

	void ImGuiLayer::EndFrame(CommandList& cmd)
	{
		{
			KOI_SCOPE("ImGui Render");
			ImGui::Render();

			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		KOI_SCOPE("ImGui Record");
		dx12::GraphicsCommandList* native = cmd.GetNative();
		ID3D12DescriptorHeap* heaps[] = { Engine::Get().GetSubsystem<Renderer>().GetDevice().ResourceHeap().GetHeap() };
		native->SetDescriptorHeaps(1, heaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), native);
	}

	void ImGuiLayer::DrawThemeMenu()
	{
		if (!ImGui::BeginMenu("Theme")) return;

		for (size_t i = 0; i < utils::kThemes.size(); ++i)
		{
			const Theme theme = static_cast<Theme>(i);
			if (ImGui::MenuItem(utils::kThemes[i].Name, nullptr, theme == m_theme))
			{
				m_theme = theme;
				utils::ApplyTheme(m_theme, m_dpiScale);
			}
		}

		ImGui::EndMenu();
	}
}
