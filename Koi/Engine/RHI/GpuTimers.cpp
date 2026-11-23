#include "Koi/Engine/RHI/GpuTimers.h"
#include "Koi/Engine/RHI/CommandList.h"

namespace koi
{
	void GpuTimers::Init(dx12::Device* device, dx12::CommandQueue* queue)
	{
		Shutdown();

		const D3D12_QUERY_HEAP_DESC heapDesc
		{
			.Type  = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
			.Count = kQueriesPerSlot * FRAMES_IN_FLIGHT,
		};
		DX_CHECK(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));
		m_heap->SetName(L"GpuTimers Query Heap");

		const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
		const CD3DX12_RESOURCE_DESC   rd = CD3DX12_RESOURCE_DESC::Buffer(u64(heapDesc.Count) * sizeof(u64));
		DX_CHECK(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rd,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_readback)));
		m_readback->SetName(L"GpuTimers Readback");

		u64 frequency = 0;
		DX_CHECK(queue->GetTimestampFrequency(&frequency));
		m_ticksToSeconds = 1.0 / f64(frequency);
	}

	void GpuTimers::Shutdown()
	{
		m_heap.Reset();
		m_readback.Reset();
		m_slots = {};
		m_last.clear();
		m_slot  = 0;
		m_depth = 0;
	}

	void GpuTimers::BeginFrame(u64 completedFence)
	{
		Slot& slot = m_slots[m_slot];
		if (slot.Resolved && completedFence >= slot.Fence)
			ReadBack(slot, m_slot);

		slot.Names.clear();
		slot.Resolved = false;
		m_depth = 0;
	}

	void GpuTimers::EndFrame(u64 fenceValue)
	{
		m_slots[m_slot].Fence = fenceValue;
		m_slot = (m_slot + 1) % FRAMES_IN_FLIGHT;
	}

	u32 GpuTimers::BeginScope(dx12::GraphicsCommandList* cmd, std::string_view name)
	{
		if (m_depth++ > 0 || !m_heap) return kInvalidScope;

		Slot& slot = m_slots[m_slot];
		if (slot.Names.size() >= kMaxScopes) return kInvalidScope;

		const u32 scope = u32(slot.Names.size());
		slot.Names.push_back(name);
		cmd->EndQuery(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_slot * kQueriesPerSlot + scope * 2);
		return scope;
	}

	void GpuTimers::EndScope(dx12::GraphicsCommandList* cmd, u32 scope)
	{
		--m_depth;
		if (scope == kInvalidScope) return;

		cmd->EndQuery(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_slot * kQueriesPerSlot + scope * 2 + 1);
	}

	void GpuTimers::Resolve(dx12::GraphicsCommandList* cmd)
	{
		Slot& slot = m_slots[m_slot];
		if (slot.Names.empty() || !m_heap) return;

		const u32 first = m_slot * kQueriesPerSlot;
		const u32 count = u32(slot.Names.size()) * 2;
		cmd->ResolveQueryData(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, count, m_readback.Get(), u64(first) * sizeof(u64));
		slot.Resolved = true;
	}

	void GpuTimers::ReadBack(const Slot& slot, u32 slotIndex)
	{
		const u64 first = u64(slotIndex) * kQueriesPerSlot;
		const D3D12_RANGE range{ first * sizeof(u64), (first + slot.Names.size() * 2) * sizeof(u64) };

		void* mapped = nullptr;
		if (FAILED(m_readback->Map(0, &range, &mapped))) return;

		const u64* ticks = static_cast<const u64*>(mapped) + first;
		const u64  base  = ticks[0];

		m_last.clear();
		for (size_t i = 0; i < slot.Names.size(); i++)
		{
			m_last.push_back({
				.Name  = slot.Names[i],
				.Start = f64(ticks[i * 2]     - base) * m_ticksToSeconds,
				.End   = f64(ticks[i * 2 + 1] - base) * m_ticksToSeconds,
			});
		}

		const D3D12_RANGE written{ 0, 0 };
		m_readback->Unmap(0, &written);
	}

	GpuStatScope::GpuStatScope(CommandList& cmd, std::string_view name)
		: m_cmd(cmd)
		, m_scope(cmd.GetGpuTimers().BeginScope(cmd.GetNative(), name))
	{
	}

	GpuStatScope::~GpuStatScope()
	{
		m_cmd.GetGpuTimers().EndScope(m_cmd.GetNative(), m_scope);
	}
}
