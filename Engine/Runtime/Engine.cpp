#include "Runtime/Engine.h"
#include "Runtime/AppBase.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/Format.h"
#include "Memory/MemoryDebug.h"
#include "Core/Profiling.h"
#include <cassert>

namespace koi
{
	Engine::Engine()
		: m_initSnapshot(GetMemSnapshot())
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);

		assert(s_instance == nullptr && "Engine already initialized");
		s_instance = this;

		KOI_LOG_INFO("Engine initializing");

		m_inputManager = std::make_unique<InputManager>();
		m_renderer     = std::make_unique<Renderer>();
	}

	Engine::~Engine()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_renderer     = nullptr;
		m_inputManager = nullptr;
		m_window.reset();
		KOI_LOG_INFO("Engine shut down");

		PrintMemSnapshot(DiffSnapshots(m_initSnapshot, GetMemSnapshot()));
		KOI_LOG_DEBUG("Engine was active for {:4f}s", GetTime());
		Logger::Shutdown();

		s_instance = nullptr;
	}

	void Engine::RunApp(IAppHost& host)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		const AppCreateDesc& desc = host.GetDesc();
		const WindowDesc& mainWndDesc = desc.WindowDesc;

		m_window.emplace(mainWndDesc);
		Window& window = *m_window;

		host.SetWindow(window);

		m_inputManager->Init(window);

		const Format depthFmt = [&]() -> Format
		{
			switch (desc.DepthBuffer)
			{
				case DepthBufferFormat::D16:    return Format::D16_UNORM;
				case DepthBufferFormat::D24S8:  return Format::D24_UNORM_S8_UINT;
				case DepthBufferFormat::None:   return Format::UNKNOWN;
				default:                        return Format::D32_FLOAT;
			}
		}();

		m_renderer->InitMainWindow(window, mainWndDesc.VSync, depthFmt);

		window.SetCloseCallback([this]() { RequestExit(); });

		// Init application
		host.HostInit();
		m_renderer->GetFrameGraph().Compile(); // One time compile to ensure user-defined render pass succeeds post-init
		MainLoop(host);
		host.HostShutdown();
	}

	void Engine::BeginFrame()
	{
		ZoneScoped;
		m_inputManager->BeginFrame();
		Window::PollEvents();
		m_renderer->BeginFrame();
	}

	void Engine::Tick(IAppHost& host)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);  // Maybe remove this tag?
		m_timer.Tick([&host](f32 dt) { host.HostTick(dt); });
		host.HostRender();
	}

	void Engine::EndFrame()
	{
		ZoneScoped;
		m_renderer->EndFrame();
	}

	void Engine::MainLoop(IAppHost& host)
	{
		m_timer.Reset();

		while (m_isRunning)
		{
			BeginFrame();
			Tick(host);
			EndFrame();
			FrameMark;
		}
	}
}
