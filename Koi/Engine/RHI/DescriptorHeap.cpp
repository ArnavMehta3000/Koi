#include "Koi/Engine/RHI/DescriptorHeap.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	void DescriptorHeap::Init(dx12::Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 capacity, bool shaderVisible)
	{
		ZoneScoped;
		KOI_ASSERT(device && capacity > 0);

		const D3D12_DESCRIPTOR_HEAP_FLAGS flags = shaderVisible
			? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
			: D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		D3D12_DESCRIPTOR_HEAP_DESC desc
		{
			.Type           = type,
			.NumDescriptors = capacity,
			.Flags          = flags,
			.NodeMask       = 0
		};

		DX_CHECK(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

		m_nextFresh     = 0;
		m_capacity      = capacity;
		m_shaderVisible = shaderVisible;
		m_stride        = device->GetDescriptorHandleIncrementSize(type);
		m_cpuStart      = m_heap->GetCPUDescriptorHandleForHeapStart();

		if (shaderVisible)
		{
			m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
		}

		m_freeList.clear();
		m_freeList.reserve(64);  // Maybe reserve capccity instead?
	}

	void DescriptorHeap::Reset()
	{
		ZoneScoped;

		m_heap = nullptr;
		m_freeList.clear();
		m_nextFresh = 0;
		m_capacity  = 0;
		m_stride    = 0;
	}

	DescriptorIndex DescriptorHeap::Allocate()
	{
		ZoneScoped;

		std::scoped_lock lock(m_mutex);

		if (!m_freeList.empty())  // Get from free list
		{
			DescriptorIndex idx = m_freeList.back();
			m_freeList.pop_back();
			return idx;
		}

		KOI_ASSERT(m_nextFresh < m_capacity, "DescriptorHeap capacity exhausted");
		if (m_nextFresh == m_capacity / 10 * 9)
		{
			KOI_LOG(RHI, Warn, "Descriptor heap (type {}) is 90% full: {} of {}", static_cast<i32>(m_heap->GetDesc().Type), m_nextFresh, m_capacity);
		}
		return m_nextFresh++;
	}

	void DescriptorHeap::Free(DescriptorIndex index)
	{
		ZoneScoped;

		// Add index to free list
		if (index == INVALID_DESCRIPTOR)
			return;

		std::scoped_lock lock(m_mutex);
		KOI_ASSERT(index < m_capacity);
		m_freeList.push_back(index);
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CpuHandle(DescriptorIndex i) const noexcept
	{
		// CD3DX12 multiplies the offset by the increment size, so pass the descriptor
		// index directly (not index * stride) or the offset gets squared by the stride.
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuStart, static_cast<INT>(i), m_stride);
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GpuHandle(DescriptorIndex i) const noexcept
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuStart, static_cast<INT>(i), m_stride);
	}
}
