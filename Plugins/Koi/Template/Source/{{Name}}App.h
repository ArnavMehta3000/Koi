#pragma once
#include "Koi/Engine/Runtime/AppBase.h"

class {{Name}}App : public koi::AppBase
{
public:
	void OnRegisterComponents() override;
	void OnInit() override;
	void OnTick(koi::f32 dt) override;
};
