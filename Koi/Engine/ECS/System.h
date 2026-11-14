#pragma once
#include "Koi/Engine/Diagnostics/CrashHandler.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"

namespace koi
{
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

		// Bracket a play session. EndPlay runs before Stop restores the edit-time world
		virtual void OnBeginPlay(World& /*world*/) {}
		virtual void OnEndPlay(World& /*world*/) {}
	};

	namespace internal
	{
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
			ECS* ecs = &world.GetECS();
			ecs->AddSystem(Phase(), [this, ecs](f32 dt)
			{
				KOI_SCOPE_TRANSIENT(Name());
				World& world = ecs->GetWorld();
				const SystemContext ctx{ world, dt };
				internal::RunGuarded(world, Name(), [&]
				{
					ecs->Each<Components...>([&](Entity e, Components&... components) { Each(ctx, e, components...); });
				});
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
			ECS* ecs = &world.GetECS();
			ecs->AddSystem(Phase(), [this, ecs](f32 dt)
			{
				KOI_SCOPE_TRANSIENT(Name());
				World& world = ecs->GetWorld();
				internal::RunGuarded(world, Name(), [&] { Run(SystemContext{ world, dt }); });
			});
		}
	};
}
