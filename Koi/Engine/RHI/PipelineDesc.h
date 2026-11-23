#pragma once
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/RHI/Types.h"
#include "Koi/Engine/RHI/DX12Common.h"
#include <span>

namespace koi
{
	// State presets mirror DirectXTK's CommonStates from D3D11

	struct RasterizerDesc
	{
		D3D12_FILL_MODE FillMode              = D3D12_FILL_MODE_SOLID;
		D3D12_CULL_MODE CullMode              = D3D12_CULL_MODE_BACK;
		bool            FrontCounterClockwise = true;  // SimpleMath is right-handed
		bool            DepthClipEnable       = true;
		bool            ConservativeRaster    = false;
		i32             DepthBias             = 0;
		f32             DepthBiasClamp        = 0.0f;
		f32             SlopeScaledDepthBias  = 0.0f;

		[[nodiscard]] static constexpr RasterizerDesc CullBack()  { return {}; }
		[[nodiscard]] static constexpr RasterizerDesc CullNone()  { return { .CullMode = D3D12_CULL_MODE_NONE }; }
		[[nodiscard]] static constexpr RasterizerDesc Wireframe() { return { .FillMode = D3D12_FILL_MODE_WIREFRAME, .CullMode = D3D12_CULL_MODE_NONE }; }

		bool operator==(const RasterizerDesc&) const = default;

		KOI_REFLECT(RasterizerDesc, FillMode, CullMode, FrontCounterClockwise, DepthClipEnable, ConservativeRaster, DepthBias, DepthBiasClamp, SlopeScaledDepthBias);
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

		[[nodiscard]] static constexpr BlendTargetDesc Opaque()     { return {}; }
		[[nodiscard]] static constexpr BlendTargetDesc AlphaBlend() { return { .BlendEnable = true, .SrcBlend = D3D12_BLEND_SRC_ALPHA, .DstBlend = D3D12_BLEND_INV_SRC_ALPHA }; }
		[[nodiscard]] static constexpr BlendTargetDesc Additive()   { return { .BlendEnable = true, .SrcBlend = D3D12_BLEND_ONE, .DstBlend = D3D12_BLEND_ONE }; }

		bool operator==(const BlendTargetDesc&) const = default;

		KOI_REFLECT(BlendTargetDesc, BlendEnable, SrcBlend, DstBlend, BlendOp, SrcBlendAlpha, DstBlendAlpha, BlendOpAlpha, WriteMask);
	};

	struct DepthStencilDesc
	{
		bool                  DepthEnable   = true;
		bool                  DepthWrite    = true;
		D3D12_COMPARISON_FUNC DepthFunc     = D3D12_COMPARISON_FUNC_LESS;
		bool                  StencilEnable = false;

		[[nodiscard]] static constexpr DepthStencilDesc Default() { return {}; }
		[[nodiscard]] static constexpr DepthStencilDesc Read()    { return { .DepthWrite = false, .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL }; }
		[[nodiscard]] static constexpr DepthStencilDesc None()    { return { .DepthEnable = false, .DepthWrite = false }; }

		bool operator==(const DepthStencilDesc&) const = default;

		KOI_REFLECT(DepthStencilDesc, DepthEnable, DepthWrite, DepthFunc, StencilEnable);
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

	struct ComputePipelineDesc
	{
		std::span<const byte> ComputeShader;
		std::string_view      DebugName;
	};
}
