#include "Koi/Engine/Graphics/RHI/CommandQueue.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include <cassert>

namespace koi
{
	void CommandQueue::Submit(CommandList& list)
	{
		assert(m_queue);
		ID3D12CommandList* raw[] = { list.GetNativeBase() };
		m_queue->ExecuteCommandLists(1, raw);
	}

	void CommandQueue::Submit(std::span<CommandList* const> lists)
	{
		assert(m_queue);
		if (lists.empty()) return;

		std::vector<ID3D12CommandList*> raw;
		raw.reserve(lists.size());
		for (CommandList* l : lists)
		{
			raw.push_back(l->GetNativeBase());
		}

		m_queue->ExecuteCommandLists(static_cast<UINT>(raw.size()), raw.data());
	}
}
