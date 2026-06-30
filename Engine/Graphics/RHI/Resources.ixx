module;
#include "Platform/Win32.h"
#include <wrl/client.h>
#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
export module Koi.Graphics.RHI.Resources;

import Koi.Graphics.Types;
import Koi.Graphics.RHI.DX12Common;

export namespace koi
{
	struct Texture
	{
		ComPtr<dx12::Resource>  Resource;
		TextureDesc             Desc;
		D3D12_RESOURCE_STATES   CurrentState   = D3D12_RESOURCE_STATE_COMMON;
		DescriptorIndex         SRVIndex       = INVALID_DESCRIPTOR;
		DescriptorIndex         UAVIndex       = INVALID_DESCRIPTOR;
		DescriptorIndex         RTVIndex       = INVALID_DESCRIPTOR;
		DescriptorIndex         DSVIndex       = INVALID_DESCRIPTOR;
		bool                    IsSwapChainImage = false;
	};

	struct Buffer
	{
		ComPtr<dx12::Resource>       Resource;
		BufferDesc                   Desc;
		D3D12_RESOURCE_STATES        CurrentState  = D3D12_RESOURCE_STATE_COMMON;
		D3D12_GPU_VIRTUAL_ADDRESS    GPUAddress    = 0;
		void*                        MappedPtr     = nullptr;
		DescriptorIndex              CBVIndex      = INVALID_DESCRIPTOR;
		DescriptorIndex              SRVIndex      = INVALID_DESCRIPTOR;
		DescriptorIndex              UAVIndex      = INVALID_DESCRIPTOR;
	};

	struct Pipeline
	{
		ComPtr<dx12::PipelineState> Pso;
		D3D_PRIMITIVE_TOPOLOGY      Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	};
}
