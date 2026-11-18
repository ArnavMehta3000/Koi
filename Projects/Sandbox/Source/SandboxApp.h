#pragma once
#include "Koi/Engine/Runtime/AppBase.h"

class SandboxApp : public koi::AppBase
{
public:
	void OnRegisterComponents() override;
	void OnRegisterSystems() override;
	void OnTick(koi::f32 dt) override;
	void OnBeginPlay() override;
	void OnSceneLoaded(std::string_view name) override;

private:
	std::string m_scene;
};
