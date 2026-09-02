#pragma once
#include "Koi/Engine/Logging/Logger.h"

// Enable to log every render-target resize. Dragging a viewport edge fires these once per
// frame, which buries everything else in the log
#define KOI_LOG_RESIZE_INFO 0

#if KOI_LOG_RESIZE_INFO
	#define KOI_LOG_RESIZE(...) KOI_LOG_DEBUG(__VA_ARGS__)
#else
	#define KOI_LOG_RESIZE(...) void(0)
#endif
