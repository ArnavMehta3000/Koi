#pragma once
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Module.h"
#include "Koi/Modules/Physics/Source/PhysicsModule.h"

class SandboxApp : public koi::AppBase
{
public:
	using Modules = koi::ModuleList<koi::PhysicsModule>;

	void OnRegisterComponents() override;
	void OnRegisterSystems() override;
	void OnRegisterModules() override { Modules::Register(); }
	void OnTick(koi::f32 dt) override;
	void OnBeginPlay() override;
	void OnSceneLoaded(std::string_view name) override;

private:
	std::string m_scene;
};
