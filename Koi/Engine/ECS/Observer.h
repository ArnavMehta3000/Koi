#pragma once
#include "Koi/Engine/ECS/System.h"
#include <type_traits>

namespace koi
{
	// flecs' built-in events
	struct OnAdd {};
	struct OnSet {};
	struct OnRemove {};

	class IObserver : public NonCopyable
	{
	public:
		virtual ~IObserver() = default;

		[[nodiscard]] virtual std::string_view Name() const = 0;

		[[nodiscard]] virtual bool FiresInEdit() const { return false; }

		virtual void Build(World& world) = 0;

		virtual void OnBeginPlay(World& /*world*/) {}
		virtual void OnEndPlay(World& /*world*/) {}
	};

	namespace internal
	{
		template<typename Event, typename Builder>
		void ApplyEvent(Builder& builder)
		{
			if constexpr      (std::is_same_v<Event, OnAdd>)    builder.event(flecs::OnAdd);
			else if constexpr (std::is_same_v<Event, OnSet>)    builder.event(flecs::OnSet);
			else if constexpr (std::is_same_v<Event, OnRemove>) builder.event(flecs::OnRemove);
			else                                                builder.template event<Event>();
		}

		[[nodiscard]] bool IsGamePlaying();

		void ReportEditObserverFailure(std::string_view observer, std::string_view what);

		template<typename Body>
		void RunObserverGuarded(World& world, std::string_view observer, Body&& body)
		{
			if (IsGamePlaying())
			{
				RunGuarded(world, observer, std::forward<Body>(body));
				return;
			}

			try
			{
				body();
			}
			catch (const std::exception& e)
			{
				ReportEditObserverFailure(observer, e.what());
			}
			catch (...)
			{
				ReportEditObserverFailure(observer, "unknown exception");
			}
		}
	}

	// Runs On() when `Event` lands on an entity that has every component in the list
	template<typename Event, typename... Components>
	class Observer : public IObserver
	{
		static_assert(sizeof...(Components) > 0, "An observer needs at least one component to match on");

	public:
		virtual void On(const Event& event, Entity e, Components&... components) = 0;

		void Build(World& world) override
		{
			auto builder = world.GetECS().observer<Components...>(std::string(Name()).c_str());
			internal::ApplyEvent<Event>(builder);

			builder.each([this](flecs::iter& it, size_t index, Components&... components)
			{
				if (!FiresInEdit() && !internal::IsGamePlaying()) return;

				static const Event kEmpty{};
				const Event* event = &kEmpty;
				if constexpr (!std::is_empty_v<Event>)
				{
					event = it.param<Event>();
				}

				KOI_SCOPE_TRANSIENT(Name());
				internal::RunObserverGuarded(World::From(it.world()), Name(), [&] { On(*event, it.entity(index), components...); });
			});
		}
	};
}
