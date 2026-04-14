#include "Platform/Application.h"
#include "Core/Logger.h"

int main()
{
	koi::Logger::Initialize();

	KOI_LOG_TRACE(koi::log::Core, "This is a {}", "trace message");


	KOI_LOG_DEBUG(koi::log::Core, "This is a {}", "debug message");
	KOI_LOG_INFO(koi::log::Core, "This is a {}", "info message");
	KOI_LOG_WARN(koi::log::Core, "This is a {}", "warning message");
	KOI_LOG_ERROR(koi::log::Core, "This is a {}", "error message");
	KOI_LOG_CRITICAL(koi::log::Core, "This is a {}", "critical message");

	koi::Application app;

	auto& mainWin = app.CreateWindow({ .Title = L"Main Viewport", .Width = 1800, .Height = 1000});
	auto& auxWin = app.CreateWindow({ .Title = L"Debug View",   .Width = 800,  .Height = 600 });
	
	while (app.Tick())
	{
		// Updating...
	}
	
	return 0;
}
