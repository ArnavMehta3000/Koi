#pragma once
// Companion macro header for Koi.Core.Logger.
// Requires: import Koi.Core.Logger; and import Koi.Core.Types;
// Also requires KOI_LOG_LEVEL which comes from Core/Config.h or Generated/Engine/LogLevel.h.

#include "Core/Types.h"   // KOI_FILE macro

#define KOI_LOG_IMPL(func, ...)                                        \
	do                                                                 \
	{                                                                  \
		auto _logger = ::koi::Logger::GetOrCreateLogger(KOI_FILE);     \
		if (_logger) _logger->func(__VA_ARGS__);                       \
	} while(0)

#define KOI_LOG_TRACE(...)    do { if constexpr (KOI_LOG_LEVEL <= 0) KOI_LOG_IMPL(trace,    __VA_ARGS__); } while(0)
#define KOI_LOG_DEBUG(...)    do { if constexpr (KOI_LOG_LEVEL <= 1) KOI_LOG_IMPL(debug,    __VA_ARGS__); } while(0)
#define KOI_LOG_INFO(...)     do { if constexpr (KOI_LOG_LEVEL <= 2) KOI_LOG_IMPL(info,     __VA_ARGS__); } while(0)
#define KOI_LOG_WARN(...)     do { if constexpr (KOI_LOG_LEVEL <= 3) KOI_LOG_IMPL(warn,     __VA_ARGS__); } while(0)
#define KOI_LOG_ERROR(...)    do { if constexpr (KOI_LOG_LEVEL <= 4) KOI_LOG_IMPL(error,    __VA_ARGS__); } while(0)
#define KOI_LOG_CRITICAL(...) do { if constexpr (KOI_LOG_LEVEL <= 5) KOI_LOG_IMPL(critical, __VA_ARGS__); } while(0)
