#include "SandboxApp.h"
#include "Components/PlayerController.h"
#include "Components/Spin.h"
#include "Components/Traveller.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <utility>

KOI_LOG_CATEGORY(Sandbox);

namespace
{
	class SandboxCommandLine final : public koi::CommandLine
	{
	public:
		SandboxCommandLine() : koi::CommandLine("Koi Sandbox")
		{
			add_option("--scene", m_scene, "Scene to start in, a name from [scenes.list]");
		}

		const std::string& GetScene() const noexcept { return m_scene; }

	private:
		std::string m_scene;
	};

	const koi::CommandLineRegistrar<SandboxCommandLine> s_cmdReg;
}

std::unique_ptr<koi::AppBase> koi::AppBase::Create(koi::CommandLine& cmd)
{
	return std::make_unique<SandboxApp>(static_cast<SandboxCommandLine&>(cmd).GetScene());
}

SandboxApp::SandboxApp(std::string startScene)
	: m_startScene(std::move(startScene))
{
}

void SandboxApp::HostInit()
{
	// Before the base loads the startup scene
	if (!m_startScene.empty())
	{
		koi::AppCreateDesc desc = GetDesc();
		if (desc.Scenes.contains(m_startScene))
		{
			desc.StartupScene = m_startScene;
			SetDesc(std::move(desc));
		}
		else
		{
			KOI_LOG(Sandbox, Error, "--scene {}: not listed in [scenes.list]", m_startScene);
		}
	}

	AppBase::HostInit();
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
