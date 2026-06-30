module;
#include "Core/Config.h"
#include "Core/Profiling.h"
#include "Graphics/RHI/DX12Common.h"
#include <cassert>
#include <string>
#include <format>
module Koi.Graphics.RHI.CommandAllocator;

import Koi.Core.Logger;
import Koi.Graphics.RHI.Device;
import Koi.Memory.MemoryTracker;
#include "Core/Logger.h"
#include "Memory/MemoryTracker.h"

namespace koi
{
	void CommandAllocator::Init(GfxDevice& device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view debugName)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			DX_CHECK(device.GetNative()->CreateCommandAllocator(
				type, IID_PPV_ARGS(&m_allocators[i])));

			if (!debugName.empty())
			{
				m_allocators[i]->SetName(std::format(L"{} [Frame {}]", debugName.data(), i).c_str());
			}
		}
	}

	void CommandAllocator::Reset(u32 frameSlot)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		assert(frameSlot < FRAMES_IN_FLIGHT);
		DX_CHECK(m_allocators[frameSlot]->Reset());
	}
}
