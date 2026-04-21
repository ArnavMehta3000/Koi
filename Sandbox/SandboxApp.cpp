#pragma once
#include "Application/AppBase.h"
#include "Engine/Engine.h"
#include "Application/WindowManager.h"
#include <thread>

class SandboxApp : public koi::AppBase
{
public:
    SandboxApp(koi::AppCreateDesc desc) : AppBase(std::move(desc)) {}

	void OnInit() override
	{
		KOI_LOG_INFO("SandboxApp::OnInit()");

		if (auto e = GetEngine())
		{
			auto& wm = e->GetWindowManager();
			m_childWindow = wm.Create(koi::WindowDesc{ .Title = "SandboxChild", .Width = 800, .Height = 600 });


			m_childRenderWindow = e->GetRenderer().AddWindow(m_childWindow, false);
			wm.Get(m_childWindow)->SetCloseCallback(
				[window = m_childWindow, renderer = &e->GetRenderer()]()
				{
					renderer->RemoveWindow(window);
				});
		}
	}

	void OnTick(koi::f32 /*dt*/)  override {}
	void OnRender() override {}
    void OnShutdown() override
	{

		KOI_LOG_INFO("SandboxApp::OnShutdown()");
	}

private:
	koi::WindowHandle m_childWindow;
	koi::RenderWindowHandle m_childRenderWindow;
};

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication(i32 argc, char** argv)
	{
		AppCreateDesc spec{};
		spec.AppName = "Sandbox";
		spec.ArgC = argc;
		spec.ArgV = argv;
		return std::make_unique<SandboxApp>(std::move(spec));
	}
}
