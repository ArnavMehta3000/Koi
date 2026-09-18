#include "SurfApp.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication([[maybe_unused]] CommandLine& cmd)
	{
		return std::make_unique<SurfApp>();
	}
}

void SurfApp::OnRegisterComponents()
{
	// Register game components here (see Samples/Sandbox/Source/Components)
}

void SurfApp::OnInit()
{
	// Register flecs systems and editor inspectors here
}

void SurfApp::OnTick(koi::f32 /*dt*/)
{
	koi::Engine& engine = *GetEngine();
	if (engine.GetSubsystem<koi::InputManager>().IsActionPressed("Quit"))
		engine.RequestExit();
}
