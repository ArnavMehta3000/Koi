#pragma once
#include "Koi/Engine/Core/LogLevel.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Logging/LoggerConfig.h"
#include <spdlog/logger.h>
#include <array>
#include <atomic>
#include <string>

namespace koi
{
	enum class LogOrigin : u8
	{
		Engine,
		Editor,
		Game,
	};

	// Always declare with KOI_LOG_CATEGORY
	struct LogCategory
	{
		LogCategory(std::string_view name, LogOrigin origin);
		u16 Id = 0;
	};

	struct LogCategoryInfo
	{
		std::string_view Name;
		LogOrigin        Origin = LogOrigin::Engine;
	};

	struct LogSourceInfo
	{
		std::string_view Path;  // as interned: __FILE__
		std::string_view Name;  // basename, for display
	};

	class Logger
	{
	public:
		static constexpr u16 kMaxCategories = 256;

		static void Init(const LoggerConfig& cfg = {});

		// Re-applies config once the project TOML is known. Rebuilds the sink set, so it must run before any worker threads exist
		static void Configure(const LoggerConfig& cfg);

		static void Shutdown();
		[[nodiscard]] static bool IsInitialized() { return s_logger != nullptr; }

		static void Flush();

		// Ids are stable for the process lifetime and usable before Init / after Shutdown
		[[nodiscard]] static u16 RegisterCategory(std::string_view name, LogOrigin origin);
		[[nodiscard]] static LogCategoryInfo GetCategory(u16 id);
		[[nodiscard]] static u16 CategoryCount();
		[[nodiscard]] static LogLevel GetCategoryLevel(u16 id) { return static_cast<LogLevel>(s_levels[id].load(std::memory_order_relaxed)); }

		[[nodiscard]] static u16 InternSource(std::string_view path);
		[[nodiscard]] static LogSourceInfo GetSource(u16 id);

		static void Log(u16 category, LogLevel level, u16 source, u32 line, std::string_view message);

		template<typename... Args>
		static void Log(u16 category, LogLevel level, u16 source, u32 line, spdlog::format_string_t<Args...> fmt, Args&&... args)
		{
			const std::string message = fmt::format(fmt, std::forward<Args>(args)...);
			Log(category, level, source, line, std::string_view(message));
		}

		[[nodiscard]] static bool ShouldLog(u16 category, LogLevel level)
		{
			return s_logger && static_cast<u8>(level) >= s_levels[category].load(std::memory_order_relaxed);
		}

	private:
		static inline spdlog::logger* s_logger = nullptr;

		// Zero is Trace, so a category logs everything until Configure says otherwise
		static constinit inline std::array<std::atomic<u8>, kMaxCategories> s_levels{};
	};
}

// At global scope only: declares koi::logs::<name> for KOI_LOG
#define KOI_LOG_CATEGORY_IN(name, origin) \
	namespace koi::logs { inline const ::koi::LogCategory name{ #name, ::koi::LogOrigin::origin }; }

#define KOI_LOG_CATEGORY(name) KOI_LOG_CATEGORY_IN(name, Game)

// KOI_LOG(Render, Info, "Created {} targets", count)
#define KOI_LOG(cat, lvl, ...)                                                                   \
	do                                                                                            \
	{                                                                                             \
		if constexpr (KOI_LOG_LEVEL <= static_cast<int>(::koi::LogLevel::lvl))                    \
		{                                                                                         \
			if (::koi::Logger::ShouldLog(::koi::logs::cat.Id, ::koi::LogLevel::lvl))              \
			{                                                                                     \
				static const ::koi::u16 _koiLogSource = ::koi::Logger::InternSource(__FILE__);    \
				::koi::Logger::Log(::koi::logs::cat.Id, ::koi::LogLevel::lvl, _koiLogSource,      \
				                   static_cast<::koi::u32>(__LINE__), __VA_ARGS__);               \
			}                                                                                     \
		}                                                                                         \
	} while (0)

#include "Koi/Engine/Logging/LogCategories.h"
