#pragma once
#include "Graphics/Types.h"
#include "Graphics/RHI/DX12Common.h"
#include <span>

namespace koi
{
	struct RasterizerDesc
	{
		D3D12_FILL_MODE FillMode              = D3D12_FILL_MODE_SOLID;
		D3D12_CULL_MODE CullMode              = D3D12_CULL_MODE_BACK;
		bool            FrontCounterClockwise = false;
		bool            DepthClipEnable       = true;
		bool            ConservativeRaster    = false;
		i32             DepthBias             = 0;
		f32             DepthBiasClamp        = 0.0f;
		f32             SlopeScaledDepthBias  = 0.0f;
	};

	struct BlendTargetDesc
	{
		bool            BlendEnable   = false;
		D3D12_BLEND     SrcBlend      = D3D12_BLEND_ONE;
		D3D12_BLEND     DstBlend      = D3D12_BLEND_ZERO;
		D3D12_BLEND_OP  BlendOp       = D3D12_BLEND_OP_ADD;
		D3D12_BLEND     SrcBlendAlpha = D3D12_BLEND_ONE;
		D3D12_BLEND     DstBlendAlpha = D3D12_BLEND_ZERO;
		D3D12_BLEND_OP  BlendOpAlpha  = D3D12_BLEND_OP_ADD;
		u8              WriteMask     = D3D12_COLOR_WRITE_ENABLE_ALL;
	};

	struct DepthStencilDesc
	{
		bool                  DepthEnable   = true;
		bool                  DepthWrite    = true;
		D3D12_COMPARISON_FUNC DepthFunc     = D3D12_COMPARISON_FUNC_LESS;
		bool                  StencilEnable = false;
	};

	struct PipelineDesc
	{
		std::span<const VertexAttribute> VertexLayout;

		std::span<const byte> VertexShader;
		std::span<const byte> PixelShader;
		std::span<const byte> GeometryShader; // optional
		std::span<const byte> HullShader;     // optional
		std::span<const byte> DomainShader;   // optional

		RasterizerDesc   Rasterizer{};
		BlendTargetDesc  BlendTargets[MAX_RENDER_TARGETS]{};
		DepthStencilDesc DepthStencil{};

		Format RenderTargetFormats[MAX_RENDER_TARGETS]{};
		u32    RenderTargetCount = 0;
		Format DepthFormat       = Format::UNKNOWN;

		// TopologyType feeds the PSO; Topology is set on the command list at draw time.
		D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		D3D_PRIMITIVE_TOPOLOGY        Topology     = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		u32              SampleCount = 1;
		std::string_view DebugName;
	};
}
