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

	class ISystem;

	namespace internal
	{
		template<typename List> struct SystemIds;
		template<typename... Ts> struct SystemIds<SystemList<Ts...>>
		{
			static std::vector<u32> Get() { return { SystemIdOf<Ts>()... }; }
		};

		struct SystemTraits
		{
			u32              Id = 0;
			std::string_view Name;
			u32              (*RegisterPhase)(ECS&) = nullptr;
			std::vector<u32> After;
			std::vector<u32> Before;
		};
	}

	// A system picks its phase and ordering with member aliases that shadow these defaults:
	// `using Phase = koi::FixedUpdate;` and `using RunsAfter = koi::SystemList<PhysicsSystem>;`
	class ISystem : public NonCopyable
	{
	public:
		using Phase      = koi::Update;
		using RunsAfter  = SystemList<>;
		using RunsBefore = SystemList<>;

		virtual ~ISystem() = default;

		[[nodiscard]] std::string_view Name() const { return m_traits.Name; }

		virtual void Build(World& world) = 0;

		// Bracket a play session. EndPlay runs before Stop restores the edit-time world
		virtual void OnBeginPlay(World& /*world*/) {}
		virtual void OnEndPlay(World& /*world*/) {}

		template<typename T>
		void SetTraits()
		{
			m_traits.Id            = internal::SystemIdOf<T>();
			m_traits.Name          = internal::TypeName<T>();
			m_traits.RegisterPhase = [](ECS& ecs) { return ecs.RegisterPhase<typename T::Phase>(); };
			m_traits.After         = internal::SystemIds<typename T::RunsAfter>::Get();
			m_traits.Before        = internal::SystemIds<typename T::RunsBefore>::Get();
		}

	protected:
		void Schedule(ECS& ecs, ECS::SystemFn run) const
		{
			KOI_ASSERT(m_traits.RegisterPhase, "System built without SystemRegistry");
			ecs.AddSystem(m_traits.RegisterPhase(ecs), { m_traits.Id, Name(), m_traits.After, m_traits.Before, std::move(run) });
		}

	private:
		internal::SystemTraits m_traits;
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
			Schedule(*ecs, [this, ecs](f32 dt)
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
			Schedule(*ecs, [this, ecs](f32 dt)
			{
				KOI_SCOPE_TRANSIENT(Name());
				World& world = ecs->GetWorld();
				internal::RunGuarded(world, Name(), [&] { Run(SystemContext{ world, dt }); });
			});
		}
	};
}
