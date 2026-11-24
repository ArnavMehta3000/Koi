// LogLevel enum + string conversion; the compile-time floor KOI_LOG_LEVEL comes from Config.h
#pragma once
#include "Koi/Engine/Core/Config.h"
#include <optional>
#include <string_view>

namespace koi
{
	// Values match spdlog::level::level_enum one-for-one
	enum class LogLevel
	{
		Trace,     // one item inside an operation, off by default
		Debug,     // what the engine did, recorded but hidden in the Log panel
		Info,      // an outcome whoever is making the game cares about
		Warn,      // it worked, with a fallback or dropped data
		Error,     // something requested didn't happen
		Critical,  // the process or the device can't continue
	};

	inline constexpr int LogLevelCount = 6;

	namespace utils
	{
		[[nodiscard]] constexpr std::string_view ToString(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Trace:    return "Trace";
			case LogLevel::Debug:    return "Debug";
			case LogLevel::Info:     return "Info";
			case LogLevel::Warn:     return "Warn";
			case LogLevel::Error:    return "Error";
			case LogLevel::Critical: return "Critical";
			default:                 return "<Unknown>";
			}
		}

		// Case-sensitive
		[[nodiscard]] constexpr std::optional<LogLevel> LogLevelFromString(std::string_view name)
		{
			for (int i = 0; i < LogLevelCount; ++i)
			{
				const auto level = static_cast<LogLevel>(i);
				if (ToString(level) == name) return level;
			}
			return std::nullopt;
		}
	}
}
