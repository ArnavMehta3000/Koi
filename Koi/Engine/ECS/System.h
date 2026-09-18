#pragma once
#include "Koi/Engine/ECS/World.h"
#include <flecs.h>
#include <string_view>

namespace koi
{
	enum class SystemPhase : u8
	{
		PreUpdate,
		FixedUpdate,
		Update,
		PostUpdate,
		PreRender,
	};

	struct SystemContext
	{
		World& World;
		f32    DeltaTime;
	};

	class ISystem : public NonCopyable
	{
	public:
		virtual ~ISystem() = default;

		[[nodiscard]] virtual std::string_view Name()  const = 0;
		[[nodiscard]] virtual SystemPhase      Phase() const { return SystemPhase::Update; }

		virtual void Build(World& world) = 0;
	};

	namespace internal
	{
		// The tag FixedUpdate systems run under. It is deliberately not a flecs Phase, so the
		// default pipeline skips them and World::Update runs them through its own
		struct FixedUpdateTag {};

		template<typename Builder>
		void ApplyPhase(Builder& builder, SystemPhase phase)
		{
			switch (phase)
			{
			case SystemPhase::PreUpdate:   builder.kind(flecs::PreUpdate);  break;
			case SystemPhase::FixedUpdate: builder.template kind<FixedUpdateTag>(); break;
			case SystemPhase::Update:      builder.kind(flecs::OnUpdate);   break;
			case SystemPhase::PostUpdate:  builder.kind(flecs::PostUpdate); break;
			case SystemPhase::PreRender:   builder.kind(flecs::PreStore);   break;
			}
		}
	}

	// Runs Each() for every entity matching the component list
	template<typename... Components>
	class System : public ISystem
	{
	public:
		virtual void Each(const SystemContext& ctx, Entity e, Components&... components) = 0;

		void Build(World& world) override
		{
			auto builder = world.GetECS().system<Components...>(std::string(Name()).c_str());
			internal::ApplyPhase(builder, Phase());

			builder.each([this](flecs::iter& it, size_t index, Components&... components)
			{
				Each(SystemContext{ World::From(it.world()), it.delta_time() }, it.entity(index), components...);
			});
		}
	};

	// Runs Run() once per tick of its phase, with no entity query
	class TaskSystem : public ISystem
	{
	public:
		virtual void Run(const SystemContext& ctx) = 0;

		void Build(World& world) override
		{
			auto builder = world.GetECS().system(std::string(Name()).c_str());
			internal::ApplyPhase(builder, Phase());

			builder.run([this](flecs::iter& it)
			{
				Run(SystemContext{ World::From(it.world()), it.delta_time() });
			});
		}
	};
}
