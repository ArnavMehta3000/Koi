#include "Application/AppBase.h"
#include "Engine/Engine.h"
#include <cassert>

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
			try
			{
				Engine engine(argc, argv);

				auto app = CreateApplication(argc, argv);
				assert(app != nullptr && "CreateApplication returned nullptr!");

				engine.RunApp(*app);
				return 0;
			}
			catch (const std::exception& e)
			{
				KOI_LOG_CRITICAL("Unhandled exception: {}", e.what());
				return 1;
			}
			catch (...)
			{
				KOI_LOG_CRITICAL("Unhandled unknown exception");
				return 2;
			}
		}
	}
}
