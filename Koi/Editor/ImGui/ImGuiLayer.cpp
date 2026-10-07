#include "Koi/Editor/ImGui/ImGuiLayer.h"
#include "Koi/Editor/ImGui/Themes.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Interop/ImGuiWidgets.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/Engine.h"

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <system_error>

// Koi/Editor/Content files, linked in as objects by xmake's utils.bin2obj rule
extern "C" const char _binary_DefaultLayout_ini_start[];
extern "C" const char _binary_DefaultLayout_ini_end[];
extern "C" const char _binary_MapleMonoRegular_ttf_start[];
extern "C" const char _binary_MapleMonoRegular_ttf_end[];
extern "C" const char _binary_MapleMonoBold_ttf_start[];
extern "C" const char _binary_MapleMonoBold_ttf_end[];
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
		static ImFont* AddEmbeddedFont(const char* start, const char* end, f32 sizePixels, ImFontConfig config)
		{
			config.FontDataOwnedByAtlas = false;
			return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<char*>(start), static_cast<int>(end - start), sizePixels, &config);
		}

		// Text face with Font Awesome merged in, so icons work in any string drawn with it
		static ImFont* AddFontWithIcons(const char* start, const char* end, f32 sizePixels)
		{
			ImFont* font = AddEmbeddedFont(start, end, sizePixels, {});

			ImFontConfig icons;
			icons.MergeMode        = true;
			icons.GlyphMinAdvanceX = sizePixels;  // fixed-width icons line up in lists
			AddEmbeddedFont(_binary_FontAwesomeSolid_ttf_start, _binary_FontAwesomeSolid_ttf_end, sizePixels * 0.85f, icons);
			return font;
		}

		static void LoadFonts(f32 sizePixels)
		{
			AddFontWithIcons(_binary_MapleMonoRegular_ttf_start, _binary_MapleMonoRegular_ttf_end, sizePixels);
			koi::utils::s_headingFont = AddFontWithIcons(_binary_MapleMonoBold_ttf_start, _binary_MapleMonoBold_ttf_end, sizePixels);
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
		RegisterSettingsHandler();
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
		ApplyAppearance();

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

	void ImGuiLayer::DrawAppearanceMenu()
	{
		if (!ImGui::BeginMenu("Appearance")) return;

		for (size_t i = 0; i < utils::kThemes.size(); ++i)
		{
			const Theme theme = static_cast<Theme>(i);
			if (ImGui::MenuItem(utils::kThemes[i].Name, nullptr, theme == m_theme))
			{
				m_theme = theme;
				ApplyAppearance();
				ImGui::MarkIniSettingsDirty();
			}
		}

		ImGui::Separator();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
		ImGui::SliderFloat("Font Scale", &m_fontScaleEdit, 0.75f, 2.0f, "%.2fx");
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			SetFontScale(m_fontScaleEdit);
		}
		if (ImGui::MenuItem("Reset Font Scale", nullptr, false, m_fontScale != 1.0f))
		{
			SetFontScale(1.0f);
		}

		ImGui::EndMenu();
	}

	void ImGuiLayer::SetFontScale(f32 scale)
	{
		m_fontScale     = std::clamp(scale, 0.75f, 2.0f);
		m_fontScaleEdit = m_fontScale;
		ImGui::GetStyle().FontScaleMain = m_fontScale;
		ImGui::MarkIniSettingsDirty();
	}

	void ImGuiLayer::SetPlayState(PlayState state)
	{
		if (state == m_playState) return;

		m_playState = state;
		ApplyAppearance();
	}

	void ImGuiLayer::ApplyAppearance()
	{
		ImGui::GetStyle().FontScaleMain = m_fontScale;
		utils::ApplyTheme(m_theme, m_dpiScale);

		if (m_playState != PlayState::Edit)
		{
			const EditorPalette& palette = EditorPalette::Get();
			utils::ApplyPlayTint(ImGui::GetStyle().Colors, m_playState == PlayState::Playing ? palette.Playing : palette.Paused);
		}
	}

	// [KoiEditor][Appearance] in imgui.ini; the theme is stored by name so reordering kThemes is safe
	void ImGuiLayer::RegisterSettingsHandler()
	{
		ImGuiSettingsHandler handler;
		handler.TypeName   = "KoiEditor";
		handler.TypeHash   = ImHashStr("KoiEditor");
		handler.UserData   = this;
		handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char* name) -> void*
		{
			return std::strcmp(name, "Appearance") == 0 ? reinterpret_cast<void*>(1) : nullptr;
		};
		handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* h, void*, const char* line)
		{
			auto* self = static_cast<ImGuiLayer*>(h->UserData);
			const std::string_view entry(line);
			if (entry.starts_with("Theme="))
			{
				const std::string_view name = entry.substr(6);
				for (size_t i = 0; i < utils::kThemes.size(); ++i)
				{
					if (name == utils::kThemes[i].Name) self->m_theme = static_cast<Theme>(i);
				}
			}
			else if (f32 scale = 1.0f; entry.starts_with("FontScale=")
				&& std::from_chars(entry.data() + 10, entry.data() + entry.size(), scale).ec == std::errc{})
			{
				self->m_fontScale     = std::clamp(scale, 0.75f, 2.0f);
				self->m_fontScaleEdit = self->m_fontScale;
			}
		};
		handler.ApplyAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h)
		{
			static_cast<ImGuiLayer*>(h->UserData)->ApplyAppearance();
		};
		handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf)
		{
			const auto* self = static_cast<const ImGuiLayer*>(h->UserData);
			buf->appendf("[KoiEditor][Appearance]\nTheme=%s\nFontScale=%.2f\n\n",
				utils::kThemes[static_cast<size_t>(self->m_theme)].Name, self->m_fontScale);
		};
		ImGui::AddSettingsHandler(&handler);
	}
}
