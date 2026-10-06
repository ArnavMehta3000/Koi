#include "SandboxApp.h"
#include "Components/PlayerController.h"
#include "Components/Spin.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Interop/ReflectInspector.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"

std::unique_ptr<koi::AppBase> koi::AppBase::Create([[maybe_unused]] koi::CommandLine& cmd)
{
	return std::make_unique<SandboxApp>();
}

void SandboxApp::OnRegisterComponents()
{
	RegisterSpinComponent();
	RegisterPlayerControllerComponent();
}

void SandboxApp::OnRegisterSystems()
{
	RegisterSpinSystem();
	RegisterPlayerControllerSystem();
}

void SandboxApp::OnInit()
{
	if (koi::IEditorContext* editor = GetEditorContext())
	{
		editor->RegisterInspector<SpinInspector>();
		editor->RegisterInspector<koi::ReflectedInspector<PlayerController>>();
	}
}

void SandboxApp::OnTick(koi::f32 /*dt*/)
{
	ZoneScoped;
	koi::Engine& engine      = *GetEngine();
	koi::InputManager& input = engine.GetSubsystem<koi::InputManager>();

	if (input.IsActionPressed("Quit"))
		engine.RequestExit();
}
