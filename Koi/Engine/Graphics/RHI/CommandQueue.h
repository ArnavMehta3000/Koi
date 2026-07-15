#pragma once
#include "Koi/Engine/Graphics/RHI/DX12Common.h"
#include <span>

namespace koi
{
	class CommandList;

	// Wrapper around an ID3D12CommandQueue owned by GfxDevice.
	class CommandQueue
	{
	public:
		CommandQueue() = default;
		explicit CommandQueue(dx12::CommandQueue* queue) noexcept : m_queue(queue) {}

		[[nodiscard]] inline dx12::CommandQueue* GetNative() const noexcept { return m_queue; }

		void Submit(CommandList& list);
		void Submit(std::span<CommandList* const> lists);

	private:
		dx12::CommandQueue* m_queue = nullptr;
	};
}
