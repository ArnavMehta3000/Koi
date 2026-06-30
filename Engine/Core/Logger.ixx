module;
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
export module Koi.Core.Logger;

export import Koi.Core.LogLevel;
export import Koi.Core.Types;

export namespace koi
{
	struct LoggerConfig
	{
		bool SpawnConsole = false;
		bool UseDebugSink = true;
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
