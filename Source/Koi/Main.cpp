#include "Platform/Application.h"
#include "Core/Logging/Logger.h"

/// \TODO: Move this somewhere insode inside the engine
struct LoggerAutoInit
{
	LoggerAutoInit() { koi::Logger::Init(); }
	~LoggerAutoInit() { koi::Logger::Shutdown(); }
};
LoggerAutoInit g_loggerAutoInit;

int main()
{
	KOI_LOG_TRACE("This is a {}", "trace message");
	KOI_LOG_DEBUG("This is a {}", "debug message");
	KOI_LOG_INFO("This is a {}", "info message");
	KOI_LOG_WARN("This is a {}", "warning message");
	KOI_LOG_ERROR("This is a {}", "error message");
	KOI_LOG_CRITICAL("This is a {}", "critical message");

	koi::Application app;

	auto& mainWin = app.CreateWindow({ .Title = L"Main Viewport", .Width = 1800, .Height = 1000});
	auto& auxWin = app.CreateWindow({ .Title = L"Debug View",   .Width = 800,  .Height = 600 });
	
	while (app.Tick())
	{
		// Updating...
	}
	
	return 0;
}
