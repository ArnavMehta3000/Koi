#include "Koi/Engine/RHI/CommandQueue.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/CommandList.h"

namespace koi
{
	void CommandQueue::Submit(CommandList& list)
	{
		ZoneScoped;
		KOI_ASSERT(m_queue);
		ID3D12CommandList* raw[] = { list.GetNativeBase() };
		m_queue->ExecuteCommandLists(1, raw);
	}

	void CommandQueue::Submit(std::span<CommandList* const> lists)
	{
		ZoneScoped;
		KOI_ASSERT(m_queue);
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
