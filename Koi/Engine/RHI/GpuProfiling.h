#pragma once
#include "Koi/Engine/RHI/DX12Common.h"
#include "Koi/Engine/Core/Hash.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/GpuTimers.h"
#include <tracy/tracy/TracyD3D12.hpp>
#include <WinPixEventRuntime/pix3.h>

namespace koi::utils
{
	[[nodiscard]] constexpr u64 GpuEventColor(std::string_view name)
	{
		return static_cast<u8>(Fnv1a(name));
	}
}

// GPU timestamp zones on the graphics queue, for Tracy, the editor Profiler and PIX. `cmd` is a koi::CommandList
#define KOI_GPU_ZONE(cmd, name)                                      \
	TracyD3D12Zone((cmd).GetProfilerCtx(), (cmd).GetNative(), name); \
	::koi::GpuStatScope ___koi_gpu_stat{ (cmd), name };              \
	PIXScopedEvent((cmd).GetNative(), ::koi::utils::GpuEventColor(name), name)

// Runtime-named variant. `name` must be a null-terminated const char* that outlives the frame
#define KOI_GPU_ZONE_TRANSIENT(cmd, varname, name)                                           \
	TracyD3D12ZoneTransient((cmd).GetProfilerCtx(), varname, (cmd).GetNative(), name, true); \
	::koi::GpuStatScope varname##_stat{ (cmd), name };                                       \
	PIXScopedEvent((cmd).GetNative(), ::koi::utils::GpuEventColor(name), "%s", name)

// PIX-only events (printf-style names), free to nest anywhere since they add no timestamps
#define KOI_GPU_EVENT(cmd, ...) \
	PIXScopedEvent((cmd).GetNative(), PIX_COLOR_INDEX(0), __VA_ARGS__)

#define KOI_GPU_BEGIN_EVENT(cmd, ...) \
	PIXBeginEvent((cmd).GetNative(), PIX_COLOR_INDEX(0), __VA_ARGS__)

#define KOI_GPU_END_EVENT(cmd) \
	PIXEndEvent((cmd).GetNative())

#define KOI_GPU_MARKER(cmd, ...) \
	PIXSetMarker((cmd).GetNative(), PIX_COLOR_INDEX(0), __VA_ARGS__)
