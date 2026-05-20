#include "Application/AppBase.h"
#include "Engine/Engine.h"
#include <cassert>
#include <iostream>

namespace koi
{
	AppBase::AppBase(const AppCreateDesc& desc)
		: m_desc(desc)
	{
		assert(s_instance == nullptr && "Application already exists!");
		s_instance = this;
		m_engine = &Engine::Get();
	}

	AppBase::~AppBase()
	{
		s_instance = nullptr;
		m_engine = nullptr;
	}

	void AppBase::Init()
	{
		KOI_LOG_INFO("Initializing application: {}", m_desc.AppName);
		OnInit();
	}

	void AppBase::Shutdown()
	{
		OnShutdown();
		KOI_LOG_INFO("Shutting down application: {}", m_desc.AppName);
	}

	namespace internal
	{
		int RunApplication(int argc, char** argv)
		{
			// Initialize the logger before the Engine so it outlives it during
			// stack unwinding — Engine::~Engine calls Logger::Shutdown(), which
			// would otherwise make IsInitialized() return false in catch blocks.
			Logger::Init();

			try
			{
				Engine engine(argc, argv);

				if (auto app = CreateApplication(argc, argv))
				{
					engine.RunApp(*app);
					return 0;
				}
				else
				{
					KOI_LOG_CRITICAL("Failed to create application");
					return -1;
				}
			}
			catch (const std::exception& e)
			{
				KOI_LOG_CRITICAL("Unhandled exception: {}", e.what());
				Logger::Shutdown();
				return 1;
			}
			catch (...)
			{
				KOI_LOG_CRITICAL("Unhandled unknown exception");
				Logger::Shutdown();
				return 2;
			}
		}
	}
}
