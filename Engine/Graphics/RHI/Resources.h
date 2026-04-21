#pragma once
#include "Graphics/Types.h"
#include "Graphics/DX12/DX12Common.h"

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
		D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;
		void* MappedPtr                      = nullptr;   // Non-null for CpuToGpu / GpuToCpu

		DescriptorIndex CbvIndex = INVALID_DESCRIPTOR;
		DescriptorIndex SrvIndex = INVALID_DESCRIPTOR;
		DescriptorIndex UavIndex = INVALID_DESCRIPTOR;
	};

	struct Pipeline
	{
		ComPtr<dx12::PipelineState> Pso;
		ComPtr<dx12::RootSignature> RootSig;
	};

	struct Shader
	{
		std::vector<koi::byte> Bytecode;
	};
}
