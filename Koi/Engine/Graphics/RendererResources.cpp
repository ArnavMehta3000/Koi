#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/PipelineDesc.h"
#include "Koi/Engine/Graphics/ShaderLoader.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	static D3D12_SAMPLER_DESC MakeLinearWrapDesc()
	{
		D3D12_SAMPLER_DESC d{};
		d.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		d.MaxLOD   = D3D12_FLOAT32_MAX;
		return d;
	}

	static D3D12_SAMPLER_DESC MakeLinearClampDesc()
	{
		D3D12_SAMPLER_DESC d{};
		d.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		d.MaxLOD   = D3D12_FLOAT32_MAX;
		return d;
	}

	static D3D12_SAMPLER_DESC MakePointClampDesc()
	{
		D3D12_SAMPLER_DESC d{};
		d.Filter   = D3D12_FILTER_MIN_MAG_MIP_POINT;
		d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		d.MaxLOD   = D3D12_FLOAT32_MAX;
		return d;
	}

	void RendererResources::Init(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		WhiteTexture = device.CreateTexture(TextureDesc
		{
			.Width     = 1,
			.Height    = 1,
			.Format    = Format::R8G8B8A8_UNORM,
			.Usage     = TextureUsage::ShaderRead,
			.DebugName = "Default White 1x1",
		});

		FlatNormalTexture = device.CreateTexture(TextureDesc
		{
			.Width     = 1,
			.Height    = 1,
			.Format    = Format::R8G8B8A8_UNORM,
			.Usage     = TextureUsage::ShaderRead,
			.DebugName = "Default Flat Normal 1x1",
		});

		// R8G8B8A8_UNORM: byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A
		// As a little-endian u32: bits[7:0]=R, bits[15:8]=G, bits[23:16]=B, bits[31:24]=A
		constexpr u32 kWhite      = 0xFFFFFFFFu;
		constexpr u32 kFlatNormal = 0xFFFF8080u;  // R=0x80, G=0x80, B=0xFF, A=0xFF

		GfxDevice::UploadContext& upload = device.GetUploadContext();
		upload.Begin();
		upload.UploadTexture(WhiteTexture,      &kWhite,      sizeof(kWhite));
		upload.UploadTexture(FlatNormalTexture, &kFlatNormal, sizeof(kFlatNormal));
		upload.End();

		LinearWrapSampler  = device.CreateSampler(MakeLinearWrapDesc());
		LinearClampSampler = device.CreateSampler(MakeLinearClampDesc());
		PointClampSampler  = device.CreateSampler(MakePointClampDesc());

		RebuildPipelines(device);
	}

	void RendererResources::Shutdown(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		device.Destroy(WhiteTexture);
		device.Destroy(FlatNormalTexture);
		device.Destroy(TonemapPipeline);
		device.Destroy(MeshOpaquePipeline);
		device.Destroy(MeshWireframePipeline);
		WhiteTexture          = {};
		FlatNormalTexture     = {};
		TonemapPipeline       = {};
		MeshOpaquePipeline    = {};
		MeshWireframePipeline = {};
		// Samplers live in the sampler heap for the device lifetime; no explicit free needed.
	}

	void RendererResources::RebuildPipelines(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		auto destroyIfValid = [&](PipelineHandle& h)
		{
			if (h) { device.Destroy(h); h = {}; }
		};

		// --- Tonemap ---
		{
			auto vsData = LoadShader("engine://Assets/Shaders/TonemapVS");
			auto psData = LoadShader("engine://Assets/Shaders/TonemapPS");
			if (vsData.empty() || psData.empty())
			{
				KOI_LOG_ERROR("RendererResources: failed to load tonemap shaders");
			}
			else
			{
				destroyIfValid(TonemapPipeline);
				TonemapPipeline = device.CreatePipeline(PipelineDesc
				{
					.VertexShader        = vsData,
					.PixelShader         = psData,
					.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
					.DepthStencil        = { .DepthEnable = false, .DepthWrite = false },
					.RenderTargetFormats = { Format::R8G8B8A8_UNORM },
					.RenderTargetCount   = 1,
					.DebugName           = "Tonemap",
				});
			}
		}

		// --- Mesh (opaque + wireframe share the same VS/PS) ---
		{
			auto vsData = LoadShader("engine://Assets/Shaders/MeshVS");
			auto psData = LoadShader("engine://Assets/Shaders/MeshPS");
			if (vsData.empty() || psData.empty())
			{
				KOI_LOG_ERROR("RendererResources: failed to load mesh shaders");
			}
			else
			{
				destroyIfValid(MeshOpaquePipeline);
				MeshOpaquePipeline = device.CreatePipeline(PipelineDesc
				{
					.VertexShader        = vsData,
					.PixelShader         = psData,
					// Right-handed view/projection: front faces wind counter-clockwise on screen
					.Rasterizer          = { .CullMode = D3D12_CULL_MODE_BACK, .FrontCounterClockwise = true },
					.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
					.RenderTargetFormats = { Format::R16G16B16A16_FLOAT },
					.RenderTargetCount   = 1,
					.DepthFormat         = Format::D32_FLOAT,
					.DebugName           = "Mesh.Opaque",
				});

				destroyIfValid(MeshWireframePipeline);
				MeshWireframePipeline = device.CreatePipeline(PipelineDesc
				{
					.VertexShader        = vsData,
					.PixelShader         = psData,
					.Rasterizer          = { .FillMode = D3D12_FILL_MODE_WIREFRAME, .CullMode = D3D12_CULL_MODE_NONE },
					.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
					.RenderTargetFormats = { Format::R16G16B16A16_FLOAT },
					.RenderTargetCount   = 1,
					.DepthFormat         = Format::D32_FLOAT,
					.DebugName           = "Mesh.Wireframe",
				});
			}
		}
	}
}
