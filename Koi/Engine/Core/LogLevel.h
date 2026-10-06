// This file contains the LogLevel generated config and the LogLevel enum  + string conversion
#pragma once
#include "Generated/Engine/LogLevel.h"  // Xmake generated config file
#include <string_view>

namespace koi
{
	// Values match spdlog::level::level_enum one-for-one
	enum class LogLevel
	{
		Trace,
		Debug,
		Info,
		Warn,
		Error,
		Critical,
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

		// Case-sensitive, falls back to Trace
		[[nodiscard]] constexpr LogLevel LogLevelFromString(std::string_view name)
		{
			for (int i = 0; i < LogLevelCount; ++i)
			{
				const auto level = static_cast<LogLevel>(i);
				if (ToString(level) == name) return level;
			}
			return LogLevel::Trace;
		}
	}
}
