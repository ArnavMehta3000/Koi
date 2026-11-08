#pragma once
#include "Koi/Engine/Graphics/RHI/DX12Common.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/GpuTimers.h"
#include <tracy/tracy/TracyD3D12.hpp>

// GPU timestamp zones on the graphics queue, for Tracy and the editor Profiler. `cmd` is a koi::CommandList
#define KOI_GPU_ZONE(cmd, name) \
	TracyD3D12Zone((cmd).GetProfilerCtx(), (cmd).GetNative(), name); \
	::koi::GpuStatScope ___koi_gpu_stat{ (cmd), name }

// Runtime-named variant. `name` must be a null-terminated const char* that outlives the frame
#define KOI_GPU_ZONE_TRANSIENT(cmd, varname, name) \
	TracyD3D12ZoneTransient((cmd).GetProfilerCtx(), varname, (cmd).GetNative(), name, true); \
	::koi::GpuStatScope varname##_stat{ (cmd), name }
