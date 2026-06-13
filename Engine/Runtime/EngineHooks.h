#pragma once
#include "Core/Config.h"
#include "Core/Types.h"
#include <functional>

namespace koi
{
	enum class EnginePhase : u8
	{
		PostInit,            // after AppBase::Init
		PreTick,             // before app OnTick
		PostTick,            // after app OnTick
		PreRender,           // before app OnRender
		PostRender,          // after app OnRender
		OnSwapchainOverlay,  // inside EndFrame, after tonemap to swapchain, before present (editor overlay UI)
#if KOI_WITH_EDITOR
		OnImGuiDraw,         // inside the editor's ImGui frame (for user ImGui overlays)
#endif
		OnShaderReload,      // editor-triggered: shader source recompiled to CSO, subscribers rebuild pipelines
		PreShutdown,         // before AppBase::Shutdown

		MAX_COUNT
	};

	using HookId = u64;
	inline constexpr HookId INVALID_HOOK_ID = 0;

	// TODO: Consider adding frame context
	using EngineHook = std::function<void()>;
}
