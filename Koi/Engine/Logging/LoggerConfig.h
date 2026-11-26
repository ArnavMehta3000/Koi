#pragma once
#include "Koi/Engine/Core/LogLevel.h"
#include "Koi/Engine/Core/Types.h"
#include <flat_map>
#include <string>

namespace koi
{
	struct LoggerConfig
	{
		std::string FilePath        = "Logs/Koi.log";   // empty disables the file sink
		LogLevel    RuntimeLevel    = LogLevel::Debug;  // default category level, on top of the compile-time KOI_LOG_LEVEL
		u32         HistoryCapacity = 16384;            // in-memory ring the editor console reads; 0 disables
		bool        SpawnConsole    = false;            // force-allocate a console on Windows
		bool        UseDebugSink    = true;             // OutputDebugString sink

		std::flat_map<std::string, LogLevel, std::less<>> CategoryLevels;  // [logging.categories], overrides RuntimeLevel
	};
}
