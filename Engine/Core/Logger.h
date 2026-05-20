// Contains the static Logger class, using spdlog under the hood

#pragma once
#include "Core/Core.h"
#include "Core/LogLevel.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace koi
{
    struct LoggerConfig
    {
        bool SpawnConsole = false; // force-allocate a console on Windows
        bool UseDebugSink = true;  // OutputDebugString sink (Win only)
    };

    class Logger
    {
    public:
        static void Init(const LoggerConfig& cfg = {});
		inline static bool IsInitialized() { return g_initialized; }
        static void Shutdown();

        static std::shared_ptr<spdlog::logger> GetLogger(std::string_view category);
        static std::shared_ptr<spdlog::logger> GetOrCreateLogger(std::string_view category);

        static void SetLevel(LogLevel level);
        static void SetCategoryLevel(std::string_view category, LogLevel level);
        static void SetPattern(const std::string& pattern);
        static void Flush();

    private:
        static std::shared_ptr<spdlog::logger> CreateLogger(std::string_view category);
        static spdlog::level::level_enum ToSpdlogLevel(LogLevel level);

	private:
		static inline bool g_initialized = false;
    };
}

#define KOI_LOG_IMPL(func, ...)                                          \
    do                                                                   \
    {                                                                    \
        auto _logger = ::koi::Logger::GetOrCreateLogger(KOI_FILE);       \
        if (_logger) _logger->func(__VA_ARGS__);                         \
    } while(0)

#define KOI_LOG_TRACE(...)    do { if constexpr (KOI_LOG_LEVEL <= 0) KOI_LOG_IMPL(trace,    __VA_ARGS__); } while(0)
#define KOI_LOG_DEBUG(...)    do { if constexpr (KOI_LOG_LEVEL <= 1) KOI_LOG_IMPL(debug,    __VA_ARGS__); } while(0)
#define KOI_LOG_INFO(...)     do { if constexpr (KOI_LOG_LEVEL <= 2) KOI_LOG_IMPL(info,     __VA_ARGS__); } while(0)
#define KOI_LOG_WARN(...)     do { if constexpr (KOI_LOG_LEVEL <= 3) KOI_LOG_IMPL(warn,     __VA_ARGS__); } while(0)
#define KOI_LOG_ERROR(...)    do { if constexpr (KOI_LOG_LEVEL <= 4) KOI_LOG_IMPL(error,    __VA_ARGS__); } while(0)
#define KOI_LOG_CRITICAL(...) do { if constexpr (KOI_LOG_LEVEL <= 5) KOI_LOG_IMPL(critical, __VA_ARGS__); } while(0)
