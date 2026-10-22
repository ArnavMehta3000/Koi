#pragma once
#include "Koi/Engine/Runtime/AppBase.h"

namespace game
{
	class SurfApp : public koi::AppBase
	{
	public:
		void OnRegisterComponents() override;
		void OnRegisterSystems() override;
	};
}
