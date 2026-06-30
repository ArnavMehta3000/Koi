module;
#include "Core/Config.h"
#include <functional>
export module Koi.Runtime.EngineHooks;

import Koi.Core.Types;

export namespace koi
{
	enum class EnginePhase : u8
	{
		PostInit,
		PreTick,
		PostTick,
		PreRender,
		PostRender,
		OnSwapchainOverlay,
#if KOI_WITH_EDITOR
		OnImGuiDraw,
#endif
		OnShaderReload,
		PreShutdown,

		MAX_COUNT
	};

	using HookId = u64;
	inline constexpr HookId INVALID_HOOK_ID = 0;

	using EngineHook = std::function<void()>;
}
