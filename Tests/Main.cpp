#define DOCTEST_CONFIG_IMPLEMENT
#include "HeadlessEngine.h"
#include "Koi/Engine/Logging/Logger.h"
#include <doctest/doctest.h>

int main(int argc, char** argv)
{
	koi::Logger::Init({ .FilePath = "", .RuntimeLevel = koi::LogLevel::Warn, .HistoryCapacity = 0, .UseDebugSink = false });

	doctest::Context context(argc, argv);
	const int result = context.run();

	test::ShutdownHeadlessEngine();
	koi::Logger::Shutdown();
	return result;
}
