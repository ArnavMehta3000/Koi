#pragma once
#include "Koi/Engine/Core/LogLevel.h"
#include "Koi/Engine/Core/Types.h"
#include <string>

namespace koi
{
	struct LoggerConfig
	{
		std::string FilePath        = "Logs/Koi.log";   // empty disables the file sink
		LogLevel    RuntimeLevel    = LogLevel::Trace;  // floor on top of the compile-time KOI_LOG_LEVEL
		u32         HistoryCapacity = 4096;             // in-memory ring the editor console reads; 0 disables
		bool        SpawnConsole    = false;            // force-allocate a console on Windows
		bool        UseDebugSink    = true;             // OutputDebugString sink
	};
}
