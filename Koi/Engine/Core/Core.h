// Includes common headers that are used throughout the engine

#pragma once
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Core/Macros.h"

#if KOI_DEBUG
#define _CRTDBG_MAP_ALLOC
	// Use the CRT Debug library to track memory leaks
	#include <crtdbg.h>
	#define KOI_DBG_TRACK_MEM() _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)
	#define KOI_DBG_TRACK_MEM_LEAK(LeakNum) _CrtSetBreakAlloc(LeakNum)
#else
	#define KOI_DBG_TRACK_MEM() do {} while(false)
	#define KOI_DBG_TRACK_MEM_LEAK(LeakNum) do {} while(false)
#endif
