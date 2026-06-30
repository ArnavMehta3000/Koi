module;
#include "Platform/Win32.h"
#include <wrl/client.h>
#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <span>
export module Koi.Graphics.RHI.CommandQueue;

import Koi.Graphics.RHI.DX12Common;

export namespace koi
{
	class CommandList;

	class CommandQueue
	{
	public:
		CommandQueue() = default;
		explicit CommandQueue(dx12::CommandQueue* queue) noexcept : m_queue(queue) {}

		[[nodiscard]] dx12::CommandQueue* GetNative() const noexcept { return m_queue; }

		void Submit(CommandList& list);
		void Submit(std::span<CommandList* const> lists);

	private:
		dx12::CommandQueue* m_queue = nullptr;
	};
}
