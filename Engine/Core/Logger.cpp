#include "Core/Logger.h"
#include "Core/StringUtils.h"
#include <filesystem>

#if KRN_OS_WINDOWS
	#include "Platform/Win32.h"
	#include <io.h>
	#include <fcntl.h>
#endif

namespace koi
{
	namespace fs = std::filesystem;
	namespace
	{
		struct LoggerData
		{
			std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> g_loggers;
			std::vector<spdlog::sink_ptr> g_sinks;
		};

		std::unique_ptr<LoggerData> g_loggerData;
		bool g_initialized;
	}

#if KRN_OS_WINDOWS
	static bool HasConsoleAttached()
	{
		return ::GetConsoleWindow() != nullptr;
	}

	static void SpawnConsole()
	{
		if (HasConsoleAttached()) return;
		if (!::AllocConsole()) return;

		FILE* f = nullptr;
		freopen_s(&f, "CONOUT$", "w", stdout);
		freopen_s(&f, "CONOUT$", "w", stderr);
		freopen_s(&f, "CONIN$", "r", stdin);
		std::ios::sync_with_stdio(true);
	}
#endif

	void Logger::Init(const LoggerConfig& cfg)
	{
		if (g_initialized) return;

		g_loggerData = std::make_unique<LoggerData>();

		// [NOTE]: Log file is Logs/Koi.log
		constexpr auto logDir = "Logs";

		fs::create_directories(logDir);

		try
		{
			const std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v";

			auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/Koi.log", true);
			fileSink->set_level(spdlog::level::trace);
			fileSink->set_pattern(pattern);
			g_loggerData->g_sinks.push_back(fileSink);

#if KRN_OS_WINDOWS
			if (cfg.UseDebugSink)
			{
				auto dbgSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
				dbgSink->set_level(spdlog::level::trace);
				dbgSink->set_pattern(pattern);
				g_loggerData->g_sinks.push_back(dbgSink);
			}

			if (cfg.SpawnConsole)
				SpawnConsole();

			// Console sink only if a console is actually attached to process
			if (HasConsoleAttached())
			{
				auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
				consoleSink->set_level(spdlog::level::trace);
				consoleSink->set_pattern(pattern);
				g_loggerData->g_sinks.push_back(consoleSink);
			}
#else
			// Non-Windows: stdout is the console
			auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			consoleSink->set_level(spdlog::level::trace);
			consoleSink->set_pattern(pattern);
			s_sinks.push_back(consoleSink);
#endif

			g_initialized = true;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			auto errMsg = fmt::format("Failed to initialize logger: {}", ex.what());
			KOI_ASSERT_MSG(false, errMsg);
		}
	}

	void Logger::Shutdown()
	{
		if (!g_initialized)
			return;

		Flush();
		g_loggerData = nullptr;
		//spdlog::shutdown();  // This keep crashing, hope that RAII cleanup works
		g_initialized = false;
	}

	std::shared_ptr<spdlog::logger> Logger::GetLogger(std::string_view category)
	{
		auto it = g_loggerData->g_loggers.find(std::string(category));
		return (it != g_loggerData->g_loggers.end()) ? it->second : nullptr;
	}

	std::shared_ptr<spdlog::logger> Logger::GetOrCreateLogger(std::string_view category)
	{
		if (auto l = GetLogger(category)) return l;
		return CreateLogger(category);
	}

	std::shared_ptr<spdlog::logger> Logger::CreateLogger(std::string_view category)
	{
		if (!g_initialized)
			return nullptr;

		try
		{
			std::string name(category);
			auto logger = std::make_shared<spdlog::logger>(name, g_loggerData->g_sinks.begin(), g_loggerData->g_sinks.end());
			logger->set_level(spdlog::level::trace);
			logger->flush_on(spdlog::level::err);

			g_loggerData->g_loggers[name] = logger;
			spdlog::register_logger(logger);
			return logger;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			auto errMsg = fmt::format("Failed to create logger: {}", ex.what());
			KOI_ASSERT_MSG(false, errMsg);
			return nullptr;
		}
	}

	void Logger::SetLevel(LogLevel level)
	{
		auto lvl = ToSpdlogLevel(level);
		for (auto& s : g_loggerData->g_sinks) s->set_level(lvl);
		for (auto& [_, logger] : g_loggerData->g_loggers) logger->set_level(lvl);
	}

	void Logger::SetCategoryLevel(std::string_view category, LogLevel level)
	{
		if (auto logger = GetOrCreateLogger(category))
			logger->set_level(ToSpdlogLevel(level));
	}

	void Logger::SetPattern(const std::string& pattern)
	{
		for (auto& s : g_loggerData->g_sinks) s->set_pattern(pattern);
	}

	void Logger::Flush()
	{
		for (auto& [_, logger] : g_loggerData->g_loggers)
		{
			logger->flush();
		}
	}

	spdlog::level::level_enum Logger::ToSpdlogLevel(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Trace:    return spdlog::level::trace;
		case LogLevel::Debug:    return spdlog::level::debug;
		case LogLevel::Info:     return spdlog::level::info;
		case LogLevel::Warn:     return spdlog::level::warn;
		case LogLevel::Error:    return spdlog::level::err;
		case LogLevel::Critical: return spdlog::level::critical;
		default:                 return spdlog::level::info;
		}
	}
}
