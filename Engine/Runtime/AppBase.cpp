#include "Runtime/AppBase.h"
#include "Runtime/Engine.h"
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
}
