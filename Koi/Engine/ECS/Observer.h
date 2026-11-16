#pragma once
#include "Koi/Engine/ECS/System.h"
#include <tuple>

namespace koi
{
	class IObserver : public NonCopyable
	{
		friend class ObserverRegistry;
	public:
		virtual ~IObserver() = default;

		[[nodiscard]] std::string_view Name() const { return m_name; }

		[[nodiscard]] virtual bool FiresInEdit() const { return false; }

		virtual void Build(World& world) = 0;

		virtual void OnBeginPlay(World& /*world*/) {}
		virtual void OnEndPlay(World& /*world*/) {}

	private:
		template<typename T>
		void SetName() { m_name = internal::TypeName<T>(); }

	private:
		std::string_view m_name;
	};

	namespace internal
	{
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
			ECS* ecs = &world.GetECS();
			ecs->Observe<Event>([this, ecs](Entity e, const Event& event)
			{
				// EndPlay drains after the engine has already left play
				constexpr bool kLifecycle = std::is_same_v<Event, BeginPlay> || std::is_same_v<Event, EndPlay>;
				if (!kLifecycle && !FiresInEdit() && !internal::IsGamePlaying()) return;

				const std::tuple<Components*...> components{ e.TryGet<Components>()... };
				if (std::apply([](auto*... c) { return ((c == nullptr) || ...); }, components)) return;

				KOI_SCOPE_TRANSIENT(Name());
				internal::RunObserverGuarded(ecs->GetWorld(), Name(), [&]
				{
					std::apply([&](auto*... c) { On(event, e, *c...); }, components);
				});
			});
		}
	};
}
