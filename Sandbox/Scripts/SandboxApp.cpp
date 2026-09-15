#include "SandboxApp.h"
#include "Components/PlayerController.h"
#include "Components/Spin.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication([[maybe_unused]] CommandLine& cmd)
	{
		return std::make_unique<SandboxApp>();
	}
}

void SandboxApp::OnRegisterComponents()
{
	RegisterSpinComponent();
	RegisterPlayerControllerComponent();
}

void SandboxApp::OnInit()
{
	RegisterSpinSystem(GetWorld().GetECS());
	RegisterPlayerControllerSystem(GetWorld().GetECS());

	if (koi::IEditorContext* editor = GetEditorContext())
	{
		editor->RegisterInspector<SpinInspector>();
		editor->RegisterInspector<PlayerControllerInspector>();
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
