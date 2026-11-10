#pragma once
#include "Koi/Engine/ECS/System.h"
#include <box3d/box3d.h>

namespace koi
{
	class PhysicsSystem : public TaskSystem
	{
	public:
		std::string_view Name() const override { return "PhysicsSystem"; }
		SystemPhase Phase() const override { return SystemPhase::FixedUpdate; }

		void Run(const SystemContext& ctx);
		void OnBeginPlay(World& world) override;
		void OnEndPlay(World& world) override;

	private:
		bool m_worldValid = false;
		b3WorldId m_b3WorldId = b3_nullWorldId;
	};
}
