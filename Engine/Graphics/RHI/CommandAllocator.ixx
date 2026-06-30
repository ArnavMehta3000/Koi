module;
#include "Platform/Win32.h"
#include <wrl/client.h>
#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <string>
export module Koi.Graphics.RHI.CommandAllocator;

import Koi.Core.Types;
import Koi.Graphics.Types;
import Koi.Graphics.RHI.DX12Common;

export namespace koi
{
	class GfxDevice;

	class CommandAllocator : NonCopyable
	{
	public:
		CommandAllocator() = default;
		~CommandAllocator() = default;

		void Init(GfxDevice& device, D3D12_COMMAND_LIST_TYPE type, std::wstring_view debugName = L"");
		void Reset(u32 frameSlot);

		[[nodiscard]] dx12::CommandAllocator* Get(u32 frameSlot) const noexcept
		{
			return m_allocators[frameSlot].Get();
		}

	private:
		std::array<ComPtr<dx12::CommandAllocator>, FRAMES_IN_FLIGHT> m_allocators;
	};
}
