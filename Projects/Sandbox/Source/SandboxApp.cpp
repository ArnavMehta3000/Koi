#include "SandboxApp.h"
#include "Components/PlayerController.h"
#include "Components/Spin.h"
#include "Components/Traveller.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
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
	RegisterTravellerObserver();
}

void SandboxApp::OnTick(koi::f32 /*dt*/)
{
	ZoneScoped;
	koi::Engine& engine      = *GetEngine();
	koi::InputManager& input = engine.GetSubsystem<koi::InputManager>();

	if (input.IsActionPressed("Quit"))
		engine.RequestExit();

	// Cycles through [scenes.list]
	if (input.IsActionPressed("NextScene"))
	{
		const auto& scenes = GetDesc().Scenes;
		auto next = scenes.upper_bound(m_scene);
		if (next == scenes.end()) next = scenes.begin();
		if (next != scenes.end()) LoadScene(next->first);
	}
}

void SandboxApp::OnBeginPlay()
{
	const koi::Entity traveller = GetWorld().Spawn("Traveller")
		.At(koi::Vector3(0.0f, 3.0f, 0.0f))
		.WithMesh("engine://Content/Meshes/Torus.kmesh")
		.With(Spin{ .Axis = koi::Vector3(1.0f, 1.0f, 0.0f), .Speed = 120.0f })
		.With(Traveller{});

	koi::World::MakePersistent(traveller);
}

void SandboxApp::OnSceneLoaded(std::string_view name)
{
	m_scene = name;
}
