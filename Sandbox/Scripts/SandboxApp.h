#pragma once
#include "Koi/Engine/Runtime/AppBase.h"

class SandboxApp : public koi::AppBase
{
public:
	void OnTick(koi::f32 dt) override;
};
