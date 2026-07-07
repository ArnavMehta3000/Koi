#pragma once
#include "Koi/Engine/Graphics/RHI/DX12Common.h"
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	class GfxDevice;

	class CommandAllocator : NonCopyable
	{
	public:
		CommandAllocator() = default;
		~CommandAllocator() = default;

		void Init(GfxDevice& device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view debugName = L"");

		// Reset the allocator for the given frame slot. GPU must be idle on that slot.
		void Reset(u32 frameSlot);

		[[nodiscard]] dx12::CommandAllocator* Get(u32 frameSlot) const noexcept
		{
			return m_allocators[frameSlot].Get();
		}

	private:
		std::array<ComPtr<dx12::CommandAllocator>, FRAMES_IN_FLIGHT> m_allocators;
	};
}
