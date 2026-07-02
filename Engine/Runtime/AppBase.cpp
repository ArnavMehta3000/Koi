#include "Runtime/AppBase.h"
#include "Runtime/Engine.h"
#include "Core/Profiling.h"
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

	void AppBase::HostInit()
	{
		KOI_LOG_INFO("Initializing application: {}", m_desc.AppName);
		OnInit();
	}

	void AppBase::HostTick(f32 dt) 
	{
		OnTick(dt);
	}

	void AppBase::HostRender() 
	{
		OnRender();
	}

	void AppBase::HostShutdown()
	{
		OnShutdown();
		KOI_LOG_INFO("Shutting down application: {}", m_desc.AppName);
	}
}
