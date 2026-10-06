#pragma once
#include "Koi/Engine/Core/LogLevel.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Logging/LoggerConfig.h"
#include <spdlog/logger.h>
#include <string>

namespace koi
{
	struct LogCategoryInfo
	{
		std::string_view Path;  // as interned: __FILE__, or a runtime-registered name
		std::string_view Name;  // basename, for display
	};

	class Logger
	{
	public:
		static void Init(const LoggerConfig& cfg = {});

		// Re-applies config once the project TOML is known. Rebuilds the sink set, so it must run before any worker threads exist
		static void Configure(const LoggerConfig& cfg);

		static void Shutdown();
		[[nodiscard]] static bool IsInitialized() { return s_logger != nullptr; }

		static void SetLevel(LogLevel level);
		static void Flush();

		// Ids are stable for the process lifetime and usable before Init / after Shutdown
		[[nodiscard]] static u16 InternCategory(std::string_view path);
		[[nodiscard]] static LogCategoryInfo GetCategory(u16 id);
		[[nodiscard]] static u16 CategoryCount();

		// Runtime: anything without __FILE__
		static void Log(LogLevel level, std::string_view category, u32 line, std::string_view message);
		static void Log(LogLevel level, u16 category, u32 line, std::string_view message);

		template<typename... Args>
		static void Log(LogLevel level, u16 category, u32 line, spdlog::format_string_t<Args...> fmt, Args&&... args)
		{
			if (!ShouldLog(level)) return;
			const std::string message = fmt::format(fmt, std::forward<Args>(args)...);
			Log(level, category, line, message);
		}

		[[nodiscard]] static bool ShouldLog(LogLevel level)
		{
			return s_logger && s_logger->should_log(static_cast<spdlog::level::level_enum>(level));
		}

	private:
		static inline spdlog::logger* s_logger = nullptr;
	};
}

#define KOI_LOG_IMPL(lvl, ...)                                                                \
	do                                                                                        \
	{                                                                                         \
		if (::koi::Logger::ShouldLog(lvl))                                                    \
		{                                                                                     \
			static const ::koi::u16 _koiLogCat = ::koi::Logger::InternCategory(__FILE__);      \
			::koi::Logger::Log(lvl, _koiLogCat, static_cast<::koi::u32>(__LINE__), __VA_ARGS__); \
		}                                                                                     \
	} while (0)

#define KOI_LOG_TRACE(...)    do { if constexpr (KOI_LOG_LEVEL <= 0) KOI_LOG_IMPL(::koi::LogLevel::Trace,    __VA_ARGS__); } while(0)
#define KOI_LOG_DEBUG(...)    do { if constexpr (KOI_LOG_LEVEL <= 1) KOI_LOG_IMPL(::koi::LogLevel::Debug,    __VA_ARGS__); } while(0)
#define KOI_LOG_INFO(...)     do { if constexpr (KOI_LOG_LEVEL <= 2) KOI_LOG_IMPL(::koi::LogLevel::Info,     __VA_ARGS__); } while(0)
#define KOI_LOG_WARN(...)     do { if constexpr (KOI_LOG_LEVEL <= 3) KOI_LOG_IMPL(::koi::LogLevel::Warn,     __VA_ARGS__); } while(0)
#define KOI_LOG_ERROR(...)    do { if constexpr (KOI_LOG_LEVEL <= 4) KOI_LOG_IMPL(::koi::LogLevel::Error,    __VA_ARGS__); } while(0)
#define KOI_LOG_CRITICAL(...) do { if constexpr (KOI_LOG_LEVEL <= 5) KOI_LOG_IMPL(::koi::LogLevel::Critical, __VA_ARGS__); } while(0)
