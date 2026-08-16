#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/PipelineDesc.h"
#include "Koi/Engine/Graphics/PipelineLibrary.h"
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

	void RendererResources::Init(GfxDevice& device, PipelineLibrary& library)
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

		KOI_LOG_DEBUG("Default textures and samplers created");
		RegisterPipelines(device, library);
	}

	void RendererResources::Shutdown(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		// Pipelines belong to the PipelineLibrary, which destroys them itself
		device.Destroy(WhiteTexture);
		device.Destroy(FlatNormalTexture);
		WhiteTexture          = {};
		FlatNormalTexture     = {};
		TonemapPipeline       = {};
		MeshOpaquePipeline    = {};
		MeshWireframePipeline = {};
		GBufferPipeline       = {};
		LightingPipeline      = {};
		// Samplers live in the sampler heap for the device lifetime; no explicit free needed.
	}

	void RendererResources::RegisterPipelines(GfxDevice& device, PipelineLibrary& library)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		KOI_LOG_DEBUG("Registering default pipelines");

		// --- Tonemap ---
		TonemapPipeline = library.Register(device, PipelineRecipe
		{
			.Name         = "Tonemap",
			.VertexShader = "engine://Assets/Shaders/TonemapVS",
			.PixelShader  = "engine://Assets/Shaders/TonemapPS",
			.Desc         =
			{
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
				.DepthStencil        = { .DepthEnable = false, .DepthWrite = false },
				.RenderTargetFormats = { Format::R8G8B8A8_UNORM },
				.RenderTargetCount   = 1,
			},
		});

		// --- Mesh (opaque + wireframe share the same VS/PS) ---
		MeshOpaquePipeline = library.Register(device, PipelineRecipe
		{
			.Name         = "Mesh.Opaque",
			.VertexShader = "engine://Assets/Shaders/MeshVS",
			.PixelShader  = "engine://Assets/Shaders/MeshPS",
			.Desc         =
			{
				// Right-handed view/projection: front faces wind counter-clockwise on screen
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_BACK, .FrontCounterClockwise = true },
				.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
				.RenderTargetFormats = { Format::R16G16B16A16_FLOAT },
				.RenderTargetCount   = 1,
				.DepthFormat         = Format::D32_FLOAT,
			},
		});

		MeshWireframePipeline = library.Register(device, PipelineRecipe
		{
			.Name         = "Mesh.Wireframe",
			.VertexShader = "engine://Assets/Shaders/MeshVS",
			.PixelShader  = "engine://Assets/Shaders/MeshPS",
			.Desc         =
			{
				.Rasterizer          = { .FillMode = D3D12_FILL_MODE_WIREFRAME, .CullMode = D3D12_CULL_MODE_NONE },
				.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
				.RenderTargetFormats = { Format::R16G16B16A16_FLOAT },
				.RenderTargetCount   = 1,
				.DepthFormat         = Format::D32_FLOAT,
			},
		});

		// --- G-buffer (deferred geometry: 3 color MRT + depth) ---
		GBufferPipeline = library.Register(device, PipelineRecipe
		{
			.Name         = "GBuffer",
			.VertexShader = "engine://Assets/Shaders/GBufferVS",
			.PixelShader  = "engine://Assets/Shaders/GBufferPS",
			.Desc         =
			{
				// Same winding/cull as the opaque mesh pass (RH, front faces wind CCW).
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_BACK, .FrontCounterClockwise = true },
				.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
				// Must match GBufferPass::CreateRenderTargets and the shader's SV_Target formats.
				.RenderTargetFormats = { Format::R8G8B8A8_UNORM, Format::R16G16B16A16_UNORM, Format::R8G8B8A8_UNORM },
				.RenderTargetCount   = 3,
				.DepthFormat         = Format::D32_FLOAT,
			},
		});

		// --- Deferred lighting (full-screen triangle: samples G-buffer -> HDR color) ---
		LightingPipeline = library.Register(device, PipelineRecipe
		{
			.Name         = "DeferredLighting",
			.VertexShader = "engine://Assets/Shaders/LightingVS",
			.PixelShader  = "engine://Assets/Shaders/LightingPS",
			.Desc         =
			{
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
				.DepthStencil        = { .DepthEnable = false, .DepthWrite = false },
				.RenderTargetFormats = { Format::R16G16B16A16_FLOAT },
				.RenderTargetCount   = 1,
			},
		});
	}
}
