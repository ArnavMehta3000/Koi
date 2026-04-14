#pragma once
#include "Core/Core.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace koi
{
	// Common categories
	namespace log
	{
		constexpr auto Core = "Core";
	}

	class Logger
	{
	public:
		enum class Level
		{
			Trace,
			Debug,
			Info,
			Warn,
			Error,
			Critical,
		};

		static void Initialize(const std::string& logDir = "Logs");
		static void Shutdown();

		static std::shared_ptr<spdlog::logger> GetLogger(const std::string& category);
		static std::shared_ptr<spdlog::logger> GetOrCreateLogger(const std::string& category);

		static void SetLevel(Level level);
		static void SetCategoryLevel(const std::string& category, Level level);
		static void SetPattern(const std::string& pattern);

		static void Flush();

	private:
		static std::shared_ptr<spdlog::logger> CreateLogger(const std::string& category);
		static spdlog::level::level_enum ToSpdlogLevel(Level level);

		static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> s_loggers;
		static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> s_consoleSink;
		static std::shared_ptr<spdlog::sinks::basic_file_sink_mt> s_fileSink;
		static bool s_initialized;
	};
}
// TODO configure this via options
#define KOI_LOG_LEVEL 0 

#define KOI_LOG_IMPL(func, category, ...)                         \
		auto logger = ::koi::Logger::GetOrCreateLogger(category); \
		if (logger) logger->func(__VA_ARGS__);                    \
	

#define KOI_LOG_TRACE(category, ...)                              \
	do { if (KOI_LOG_LEVEL <= 0) {                                \
		KOI_LOG_IMPL(trace, category, __VA_ARGS__);               \
	} } while(0)

#define KOI_LOG_DEBUG(category, ...)                              \
	do { if (KOI_LOG_LEVEL <= 1) {                                \
		KOI_LOG_IMPL(debug, category, __VA_ARGS__);               \
	} } while(0)

#define KOI_LOG_INFO(category, ...)                               \
	do{ if (KOI_LOG_LEVEL <= 2) {                                 \
		KOI_LOG_IMPL(info, category, __VA_ARGS__);                \
	} } while(0)

#define KOI_LOG_WARN(category, ...)                               \
	do { if (KOI_LOG_LEVEL <= 3) {                                \
		KOI_LOG_IMPL(warn, category, __VA_ARGS__);               \
	} } while(0)

#define KOI_LOG_ERROR(category, ...)                              \
	do { if (KOI_LOG_LEVEL <= 4) {                                \
		KOI_LOG_IMPL(error, category, __VA_ARGS__);               \
	} } while(0)

#define KOI_LOG_CRITICAL(category, ...)                           \
	do { if (KOI_LOG_LEVEL <= 5) {                                \
		KOI_LOG_IMPL(critical, category, __VA_ARGS__);             \
	} } while(0)