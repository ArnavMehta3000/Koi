// Includes auto-generated headers acting as global configuration

#pragma once
#include "Generated/Engine/Version.h"  // Generated at build time
#include "Generated/Engine/LogLevel.h"  // Generated at build time
#include "Generated/Engine/EditorConfig.h"  // Generated at build time

#if defined(NDEBUG)
	#define KOI_DEBUG 0
	#define KOI_RELEASE 1
#else
	#define KOI_DEBUG 1
	#define KOI_RELEASE 0
#endif

#if KOI_ENABLE_PROFILING
	#define KOI_PROFILING 1
#else
	#define KOI_PROFILING 0
#endif
