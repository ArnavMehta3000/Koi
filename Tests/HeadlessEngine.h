#pragma once

namespace koi { class Engine; }

namespace test
{
	// One per process, created on first use with engine:// mounted: device creation is too slow to repeat per case
	koi::Engine& HeadlessEngine();

	// Called by main after the run, so the device goes before the singletons it logs through
	void ShutdownHeadlessEngine();
}
