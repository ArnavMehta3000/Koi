#include "{{Name}}App.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"

std::unique_ptr<koi::AppBase> koi::AppBase::Create([[maybe_unused]] koi::CommandLine& cmd)
{
	return std::make_unique<{{Name}}App>();
}

void {{Name}}App::OnRegisterComponents()
{
	// Register game components here (see Samples/Sandbox/Source/Components)
}

void {{Name}}App::OnInit()
{
	// Register flecs systems and editor inspectors here
}

void {{Name}}App::OnTick(koi::f32 /*dt*/)
{
	koi::Engine& engine = *GetEngine();
	if (engine.GetSubsystem<koi::InputManager>().IsActionPressed("Quit"))
		engine.RequestExit();
}
