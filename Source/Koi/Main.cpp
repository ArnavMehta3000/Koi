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

	koi::Window main({ .Title = "Main View", .Width = 1920, .Height = 1080 });
	koi::Window side({ .Title = "Inspector", .Mode = koi::WindowMode::Borderless, .Width = 800, .Height = 600,  });

	while (!main.ShouldClose() && !side.ShouldClose())
	{
		koi::Window::PollEvents();
		// render to both swapchains, present each
	}
}
