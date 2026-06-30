module;
#include "Generated/Engine/LogLevel.h"
#include <string_view>
export module Koi.Core.LogLevel;

export namespace koi
{
	enum class LogLevel
	{
		Trace,
		Debug,
		Info,
		Warn,
		Error,
		Critical,
	};

	inline std::string_view ToString(LogLevel level)
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
}
