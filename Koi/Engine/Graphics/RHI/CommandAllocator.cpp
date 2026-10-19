#include "Koi/Engine/Graphics/RHI/CommandAllocator.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include <spdlog/fmt/xchar.h>

namespace koi
{
	void CommandAllocator::Init(GfxDevice& device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view debugName)
	{
		ZoneScoped;

		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			DX_CHECK(device.GetNative()->CreateCommandAllocator(
				type, IID_PPV_ARGS(&m_allocators[i])));

			if (!debugName.empty())
			{
				m_allocators[i]->SetName(fmt::format(L"{} [Frame {}]", debugName.data(), i).c_str());
			}
		}
	}

	void CommandAllocator::Reset(u32 frameSlot)
	{
		ZoneScoped;

		KOI_ASSERT(frameSlot < FRAMES_IN_FLIGHT);
		DX_CHECK(m_allocators[frameSlot]->Reset());
	}
}
