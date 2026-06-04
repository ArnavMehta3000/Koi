#pragma once
#include "Core/Types.h"
#include <functional>

namespace koi
{
	enum class EnginePhase : u8
	{
		PostInit,            // after AppBase::Init (game OnInit ran, GPU resources up)
		PreTick,             // each frame, before app OnTick
		PostTick,            // each frame, after app OnTick
		PreRender,           // each frame, before app OnRender
		PostRender,          // each frame, after app OnRender
		OnSwapchainOverlay,  // inside EndFrame, after tonemap to swapchain, before present (editor overlay UI)
		OnShaderReload,      // editor-triggered: shader source recompiled to CSO, subscribers rebuild pipelines
		PreShutdown,         // before AppBase::Shutdown

		MAX_COUNT
	};

	using HookId = u64;
	inline constexpr HookId INVALID_HOOK_ID = 0;

	// TODO: Consider adding frame context
	using EngineHook = std::function<void()>;
}
