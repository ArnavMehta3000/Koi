#pragma once
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Module.h"
#include "Koi/Modules/Physics/Source/PhysicsModule.h"

namespace game
{
	class SurfApp : public koi::AppBase
	{
	public:
		using Modules = koi::ModuleList<koi::PhysicsModule>;

		void OnRegisterComponents() override;
		void OnRegisterSystems() override;
		void OnRegisterModules() override { Modules::Register(); }
	};
}
