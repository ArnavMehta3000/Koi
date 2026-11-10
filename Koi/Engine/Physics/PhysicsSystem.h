#pragma once
#include "Koi/Engine/ECS/System.h"
#include <box3d/id.h>

namespace koi
{
	struct PhysicsSettings;

	class PhysicsSystem : public TaskSystem
	{
	public:
		~PhysicsSystem() override;
		[[nodiscard]] std::string_view Name() const override { return "PhysicsSystem"; }
		[[nodiscard]] SystemPhase Phase() const override { return SystemPhase::FixedUpdate; }

		void Run(const SystemContext& ctx) override;
		void OnBeginPlay(World& world) override;
		void OnEndPlay(World& world) override;

	private:
		void CreatePhysicsWorld(const PhysicsSettings& settings);
		void DestroyPhysicsWorld();
		void SetupBodies(World& world);

	private:
		b3WorldId m_b3WorldId = b3_nullWorldId;
	};
}
