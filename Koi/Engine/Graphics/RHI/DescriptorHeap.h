#pragma once
#include "Koi/Engine/Graphics/RHI/DX12Common.h"
#include "Koi/Engine/Graphics/Types.h"
#include <vector>
#include <mutex>

namespace koi
{
	class DescriptorHeap : public NonCopyable
	{
	public:
		DescriptorHeap() = default;
		~DescriptorHeap() = default;

		[[nodiscard]] dx12::DescriptorHeap* GetHeap() const noexcept { return m_heap.Get(); }
		[[nodiscard]] u32 GetCapacity() const noexcept { return m_capacity; }

		void Init(dx12::Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 capacity, bool shaderVisible);
		void Reset();

		[[nodiscard]] DescriptorIndex Allocate();
		void Free(DescriptorIndex index);
		[[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle(DescriptorIndex i) const noexcept;
		[[nodiscard]] CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle(DescriptorIndex i) const noexcept;

	private:
		ComPtr<dx12::DescriptorHeap>  m_heap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuStart;
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_gpuStart;
		u32                           m_stride        = 0;
		u32                           m_capacity      = 0;
		u32                           m_nextFresh     = 0;
		bool                          m_shaderVisible = false;
		std::vector<DescriptorIndex>  m_freeList;  // Resued slots
		std::mutex                    m_mutex;
	};
}
