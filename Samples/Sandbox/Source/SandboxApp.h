#pragma once
#include "Koi/Engine/Runtime/AppBase.h"

class SandboxApp : public koi::AppBase
{
public:
	void OnRegisterComponents() override;
	void OnRegisterSystems() override;
	void OnInit() override;
	void OnTick(koi::f32 dt) override;
};
