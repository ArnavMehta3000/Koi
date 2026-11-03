#include "HeadlessEngine.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <doctest/doctest.h>
#include <optional>

using namespace koi;

namespace
{
	struct Counters
	{
		u32  Calls     = 0;
		u32  Begun     = 0;
		u32  Ended     = 0;
		f32  LastDelta = 0.0f;
		bool Alive     = false;
		bool HookSawDeadSystem = false;
	};

	Counters s_fixed;
	Counters s_frame;

	struct Probe
	{
		u32 Value = 0;
	};

	class CountingSystem : public TaskSystem
	{
	public:
		CountingSystem(SystemPhase phase, Counters* counters) : m_phase(phase), m_counters(counters) { m_counters->Alive = true; }
		~CountingSystem() override { m_counters->Alive = false; }

		[[nodiscard]] std::string_view Name()  const override { return m_phase == SystemPhase::FixedUpdate ? "CountFixed" : "CountFrame"; }
		[[nodiscard]] SystemPhase      Phase() const override { return m_phase; }

		void Build(World& world) override
		{
			TaskSystem::Build(world);
			if (m_phase != SystemPhase::FixedUpdate) return;

			world.GetECS().component<Probe>().on_remove([counters = m_counters](Entity, Probe&)
			{
				if (!counters->Alive) counters->HookSawDeadSystem = true;
			});
		}

		void Run(const SystemContext& ctx) override
		{
			++m_counters->Calls;
			m_counters->LastDelta = ctx.DeltaTime;
		}

		void OnBeginPlay(World&) override { ++m_counters->Begun; }
		void OnEndPlay(World&) override   { ++m_counters->Ended; }

	private:
		SystemPhase m_phase;
		Counters*   m_counters;
	};

	// The registry is process-wide, so these systems join every later World too; they only count
	void RegisterCountingSystems()
	{
		static const bool s_registered = []
		{
			SystemRegistry::Get().Register<CountingSystem>(SystemPhase::FixedUpdate, &s_fixed);
			SystemRegistry::Get().Register<CountingSystem>(SystemPhase::Update, &s_frame);
			return true;
		}();
		(void)s_registered;
	}

	struct PlayingWorld
	{
		PlayingWorld()
		{
			test::HeadlessEngine().SetPlayState(PlayState::Playing);
			RegisterCountingSystems();
			s_fixed = {};
			s_frame = {};
			World.emplace();
		}

		~PlayingWorld() { test::HeadlessEngine().SetPlayState(PlayState::Edit); }

		std::optional<koi::World> World;
	};
}

TEST_CASE("FixedUpdate systems run once per fixed step and nowhere else")
{
	PlayingWorld w;

	w.World->Update(3.5f * World::kFixedStep);
	CHECK(s_fixed.Calls == 3);
	CHECK(s_fixed.LastDelta == doctest::Approx(World::kFixedStep));
	CHECK(s_frame.Calls == 1);

	// The half step left over joins the next frame's time
	w.World->Update(0.5f * World::kFixedStep);
	CHECK(s_fixed.Calls == 4);
	CHECK(s_frame.Calls == 2);
}

TEST_CASE("A frame shorter than a fixed step runs no fixed systems")
{
	PlayingWorld w;

	w.World->Update(0.25f * World::kFixedStep);
	CHECK(s_fixed.Calls == 0);
	CHECK(s_frame.Calls == 1);
}

TEST_CASE("Nothing runs outside play mode")
{
	PlayingWorld w;
	test::HeadlessEngine().SetPlayState(PlayState::Edit);

	w.World->Update(2.0f * World::kFixedStep);
	CHECK(s_fixed.Calls == 0);
	CHECK(s_frame.Calls == 0);
}

TEST_CASE("Play transitions reach every system and drop leftover fixed time")
{
	PlayingWorld w;

	w.World->BeginPlay();
	CHECK(s_fixed.Begun == 1);
	CHECK(s_frame.Begun == 1);

	w.World->Update(0.75f * World::kFixedStep);
	w.World->EndPlay();
	CHECK(s_fixed.Ended == 1);
	CHECK(s_frame.Ended == 1);

	// Without the reset, the 0.75 carried over would complete a step here
	w.World->BeginPlay();
	w.World->Update(0.5f * World::kFixedStep);
	CHECK(s_fixed.Calls == 0);
}

TEST_CASE("Component hooks fired by world teardown still find their systems alive")
{
	PlayingWorld w;
	(void)w.World->GetECS().entity().set<Probe>({});

	w.World.reset();
	CHECK_FALSE(s_fixed.HookSawDeadSystem);
}
