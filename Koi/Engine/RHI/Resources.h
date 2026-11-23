#pragma once
#include "Koi/Engine/RHI/Types.h"
#include "Koi/Engine/RHI/DX12Common.h"

namespace koi
{
	struct Texture
	{
		ComPtr<dx12::Resource> Resource;
		TextureDesc Desc;

		D3D12_RESOURCE_STATES CurrentState = D3D12_RESOURCE_STATE_COMMON;

		DescriptorIndex SRVIndex = INVALID_DESCRIPTOR;
		DescriptorIndex UAVIndex = INVALID_DESCRIPTOR;
		DescriptorIndex RTVIndex = INVALID_DESCRIPTOR;
		DescriptorIndex DSVIndex = INVALID_DESCRIPTOR;

		bool IsSwapChainImage = false;  // Lifetime is managed by the swap chain
	};

	struct Buffer
	{
		ComPtr<dx12::Resource> Resource;
		BufferDesc Desc;
		D3D12_RESOURCE_STATES CurrentState  = D3D12_RESOURCE_STATE_COMMON;
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
		void* MappedPtr                      = nullptr;   // Non-null for CpuToGpu / GpuToCpu

		DescriptorIndex CBVIndex = INVALID_DESCRIPTOR;
		DescriptorIndex SRVIndex = INVALID_DESCRIPTOR;
		DescriptorIndex UAVIndex = INVALID_DESCRIPTOR;
	};

	struct Pipeline
	{
		ComPtr<dx12::PipelineState> Pso;
		D3D_PRIMITIVE_TOPOLOGY      Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		bool                        IsCompute = false;
	};
}
