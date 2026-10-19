#pragma once
#include "Koi/Engine/Diagnostics/CrashHandler.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"

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

		// False when the body threw, or was skipped because an earlier system already had
		template<typename Body>
		bool RunGuarded(World& world, std::string_view system, Body&& body)
		{
			if (world.HasSystemFailure()) return false;

			try
			{
				body();
				return true;
			}
			catch (const std::exception& e)
			{
				world.ReportSystemFailure(system, e.what(), CrashHandler::TraceOf(e));
			}
			catch (...)
			{
				world.ReportSystemFailure(system, "unknown exception", CrashHandler::LastThrowTrace());
			}
			return false;
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

			// run() wraps every matched table, so the profiler sees one entry per system, not per entity
			builder.run([this](flecs::iter& it)
			{
				KOI_SCOPE_TRANSIENT(Name());
				if (!internal::RunGuarded(World::From(it.world()), Name(), [&] { while (it.next()) it.each(); }))
				{
					it.fini();
				}
			},
			[this](flecs::iter& it, size_t index, Components&... components)
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
				KOI_SCOPE_TRANSIENT(Name());
				World& world = World::From(it.world());
				internal::RunGuarded(world, Name(), [&] { Run(SystemContext{ world, it.delta_time() }); });
			});
		}
	};
}
