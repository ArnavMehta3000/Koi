module;
#include "Platform/Win32.h"
#include <wrl/client.h>
#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <vector>
#include <mutex>
export module Koi.Graphics.RHI.DescriptorHeap;

import Koi.Core.Types;
import Koi.Graphics.Types;
import Koi.Graphics.RHI.DX12Common;

export namespace koi
{
	class DescriptorHeap : public NonCopyable
	{
	public:
		DescriptorHeap() = default;
		~DescriptorHeap() = default;

		[[nodiscard]] dx12::DescriptorHeap* GetHeap()     const noexcept { return m_heap.Get(); }
		[[nodiscard]] u32                   GetCapacity() const noexcept { return m_capacity;   }

		void Init(dx12::Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 capacity, bool shaderVisible);
		void Reset();

		[[nodiscard]] DescriptorIndex              Allocate();
		void                                       Free(DescriptorIndex index);
		[[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle(DescriptorIndex i) const noexcept;
		[[nodiscard]] CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle(DescriptorIndex i) const noexcept;

	private:
		ComPtr<dx12::DescriptorHeap>  m_heap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
		u32                           m_stride        = 0;
		u32                           m_capacity      = 0;
		u32                           m_nextFresh     = 0;
		bool                          m_shaderVisible = false;
		std::vector<DescriptorIndex>  m_freeList;
		std::mutex                    m_mutex;
	};
}
