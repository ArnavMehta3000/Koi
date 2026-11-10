#include "HeadlessEngine.h"
#include "Koi/Engine/ECS/ObserverRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <doctest/doctest.h>
#include <optional>
#include <stdexcept>

using namespace koi;

namespace
{
	struct Health { i32 Value = 0; };
	struct Armor  { i32 Value = 0; };
	struct Doomed { bool Yes = true; };

	struct Hit   { i32 Damage = 0; };
	struct Ping  {};
	struct Boom  {};
	struct Never { i32 Unused = 0; };

	struct Counts
	{
		u32 HealthSet     = 0;
		i32 LastSetValue  = 0;
		u32 HealthAdded   = 0;
		u32 Hits          = 0;
		u32 ArmoredHits   = 0;
		u32 Pings         = 0;
		u32 HitsSeenInRun = 0;
		bool EmitFromSystem = false;
		Entity Target;
	};

	Counts s_counts;

	class HealthSetObserver final : public Observer<OnSet, const Health>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "Test.HealthSet"; }
		void On(const OnSet&, Entity, const Health& health) override
		{
			++s_counts.HealthSet;
			s_counts.LastSetValue = health.Value;
		}
	};

	class HealthAddedInEdit final : public Observer<OnAdd, const Health>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "Test.HealthAdded"; }
		[[nodiscard]] bool FiresInEdit() const override { return true; }
		void On(const OnAdd&, Entity, const Health&) override { ++s_counts.HealthAdded; }
	};

	class HitObserver final : public Observer<Hit, Health>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "Test.Hit"; }
		void On(const Hit& hit, Entity, Health& health) override
		{
			++s_counts.Hits;
			health.Value -= hit.Damage;
		}
	};

	class ArmoredHitObserver final : public Observer<Hit, const Health, const Armor>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "Test.ArmoredHit"; }
		void On(const Hit&, Entity, const Health&, const Armor&) override { ++s_counts.ArmoredHits; }
	};

	class PingObserver final : public Observer<Ping, const Health>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "Test.Ping"; }
		void On(const Ping&, Entity, const Health&) override { ++s_counts.Pings; }
	};

	class BoomObserver final : public Observer<Boom, const Doomed>
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "Test.Boom"; }
		[[nodiscard]] bool FiresInEdit() const override { return true; }
		void On(const Boom&, Entity, const Doomed&) override { throw std::runtime_error("boom"); }
	};

	class EmitterSystem final : public TaskSystem
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "Test.Emitter"; }
		void Run(const SystemContext& ctx) override
		{
			if (!s_counts.EmitFromSystem) return;

			ctx.World.Emit(s_counts.Target, Hit{ .Damage = 1 });
			s_counts.HitsSeenInRun = s_counts.Hits;
		}
	};

	// The registries are process-wide, so these join every later World too
	void RegisterTestObservers()
	{
		static const bool s_registered = []
		{
			ObserverRegistry& observers = ObserverRegistry::Get();
			observers.Register<HealthSetObserver>();
			observers.Register<HealthAddedInEdit>();
			observers.Register<HitObserver>();
			observers.Register<ArmoredHitObserver>();
			observers.Register<PingObserver>();
			observers.Register<BoomObserver>();
			SystemRegistry::Get().Register<EmitterSystem>();
			return true;
		}();
		(void)s_registered;
	}

	struct TestWorld
	{
		explicit TestWorld(PlayState state = PlayState::Playing)
		{
			test::HeadlessEngine().SetPlayState(state);
			RegisterTestObservers();
			s_counts = {};
			World.emplace();
		}

		~TestWorld() { test::HeadlessEngine().SetPlayState(PlayState::Edit); }

		std::optional<koi::World> World;
	};
}

TEST_CASE("Built-in events reach observers while playing")
{
	TestWorld w;
	(void)w.World->GetECS().entity().set<Health>({ 7 });

	CHECK(s_counts.HealthSet == 1);
	CHECK(s_counts.LastSetValue == 7);
	CHECK(s_counts.HealthAdded == 1);
}

TEST_CASE("Outside play only observers that opt in fire")
{
	TestWorld w(PlayState::Edit);
	(void)w.World->GetECS().entity().set<Health>({ 7 });

	CHECK(s_counts.HealthSet == 0);
	CHECK(s_counts.HealthAdded == 1);
}

TEST_CASE("Paused counts as outside play")
{
	TestWorld w(PlayState::Paused);
	(void)w.World->GetECS().entity().set<Health>({ 7 });

	CHECK(s_counts.HealthSet == 0);
	CHECK(s_counts.HealthAdded == 1);
}

TEST_CASE("Emit delivers the payload only to entities with the observed components")
{
	TestWorld w;
	const Entity target = w.World->GetECS().entity().set<Health>({ 10 });
	const Entity bystander = w.World->GetECS().entity().set<Armor>({ 3 });

	w.World->Emit(target, Hit{ .Damage = 4 });
	w.World->Emit(bystander, Hit{ .Damage = 4 });

	CHECK(s_counts.Hits == 1);
	CHECK(target.get<Health>().Value == 6);
}

TEST_CASE("An observer matching several components fires once per event")
{
	TestWorld w;
	const Entity knight = w.World->GetECS().entity().set<Health>({ 10 }).set<Armor>({ 5 });

	w.World->Emit(knight, Hit{ .Damage = 1 });

	CHECK(s_counts.ArmoredHits == 1);
	CHECK(s_counts.Hits == 1);
}

TEST_CASE("Tag events need no payload")
{
	TestWorld w;
	const Entity e = w.World->GetECS().entity().set<Health>({ 1 });

	w.World->Emit(e, Ping{});
	w.World->Emit<Ping>(e);

	CHECK(s_counts.Pings == 2);
}

TEST_CASE("Emitting an event nobody observes, or at a dead entity, does nothing")
{
	TestWorld w;
	const Entity e = w.World->GetECS().entity().set<Health>({ 1 });

	CHECK_NOTHROW(w.World->Emit(e, Never{ .Unused = 1 }));

	const Entity gone = w.World->GetECS().entity().set<Health>({ 1 });
	gone.destruct();
	CHECK_NOTHROW(w.World->Emit(gone, Hit{ .Damage = 1 }));
	CHECK(s_counts.Hits == 0);
}

TEST_CASE("An event emitted inside a system fires after the system, not during it")
{
	TestWorld w;
	s_counts.Target = w.World->GetECS().entity().set<Health>({ 10 });
	s_counts.EmitFromSystem = true;

	w.World->Update(0.25f * World::kFixedStep);

	CHECK(s_counts.HitsSeenInRun == 0);
	CHECK(s_counts.Hits == 1);
	CHECK(s_counts.Target.get<Health>().Value == 9);
}

TEST_CASE("A throwing observer surfaces through Update while playing")
{
	TestWorld w;
	const Entity e = w.World->GetECS().entity().set<Doomed>({});

	w.World->Emit<Boom>(e);
	CHECK(w.World->HasSystemFailure());
	CHECK_THROWS(w.World->Update(World::kFixedStep));
	CHECK_FALSE(w.World->HasSystemFailure());
}

TEST_CASE("A throwing observer outside play is logged, not parked")
{
	TestWorld w(PlayState::Edit);
	const Entity e = w.World->GetECS().entity().set<Doomed>({});

	CHECK_NOTHROW(w.World->Emit<Boom>(e));
	CHECK_FALSE(w.World->HasSystemFailure());
}

TEST_CASE("Observers follow their World through a move")
{
	TestWorld w;
	koi::World moved = std::move(*w.World);
	const Entity e = moved.GetECS().entity().set<Health>({ 10 });

	moved.Emit(e, Hit{ .Damage = 2 });

	CHECK(s_counts.Hits == 1);
	CHECK(e.get<Health>().Value == 8);
}
