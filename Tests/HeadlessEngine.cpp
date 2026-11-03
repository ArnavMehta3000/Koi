#include "HeadlessEngine.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Core/Config.h"
#include <memory>

namespace
{
	std::unique_ptr<koi::Engine> s_engine;
}

koi::Engine& test::HeadlessEngine()
{
	if (!s_engine)
	{
		// The asset index scans the mounts when the engine comes up, so they go first
		koi::VirtualFileSystem::Get().Mount("engine", std::make_unique<koi::FilesystemMount>(koi::fs::path(KOI_ENGINE_DIR)));
		s_engine = std::make_unique<koi::Engine>(koi::Engine::Desc{ .Headless = true });
	}
	return *s_engine;
}

void test::ShutdownHeadlessEngine()
{
	s_engine.reset();
}
