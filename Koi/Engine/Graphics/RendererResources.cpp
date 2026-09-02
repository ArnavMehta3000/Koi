#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/Procedural/MeshGenerators.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	// Not a real file: the fallback is generated, and only needs a key nothing else can claim
	constexpr std::string_view kFallbackTextureUri = "koi://fallback/checker";

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

	static D3D12_SAMPLER_DESC MakeShadowCompareDesc()
	{
		D3D12_SAMPLER_DESC d{};
		d.Filter         = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		d.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		// White border = far plane, so anything outside the map compares as lit
		d.BorderColor[0] = d.BorderColor[1] = d.BorderColor[2] = d.BorderColor[3] = 1.0f;
		d.MaxLOD         = D3D12_FLOAT32_MAX;
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
		ShadowSampler      = device.CreateSampler(MakeShadowCompareDesc());

		KOI_LOG_DEBUG("Default textures and samplers created");
		RegisterPipelines(device, library);
	}

	void RendererResources::RegisterFallbackAssets(GfxDevice& device)
	{
		// This function cannot be called in Init since it requires the AssetRegistry subsystem, which is constructed after the Renderer (and these resources)
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		AssetRegistry& assets = Engine::Get().GetSubsystem<AssetRegistry>();

		constexpr i32 kSize = 16;
		constexpr i32 kTile = 4;

		ImageAsset checker
		{
			.Name     = "Fallback",
			.Path     = std::string(kFallbackTextureUri),
			.Width    = kSize,
			.Height   = kSize,
			.Channels = 4,
		};
		checker.Pixels.resize(static_cast<size_t>(kSize) * kSize * 4);

		// Fallback textue: Magenta/Black checkerboard texture
		for (i32 y = 0; y < kSize; ++y)
		{
			for (i32 x = 0; x < kSize; ++x)
			{
				const bool lit = ((x / kTile) + (y / kTile)) % 2 == 0;
				koi::byte* px  = checker.Pixels.data() + (static_cast<size_t>(y) * kSize + x) * 4;

				px[0] = lit ? koi::byte(0xFF) : koi::byte(0x14);
				px[1] = koi::byte(0x00);
				px[2] = lit ? koi::byte(0xFF) : koi::byte(0x14);
				px[3] = koi::byte(0xFF);
			}
		}

		AssetCache<GPUTexture>& textures = assets.Cache<GPUTexture>();
		FallbackTexture = textures.AcquireFrom(device, AssetKey::FromUri(std::string(kFallbackTextureUri)), checker);
		textures.SetFallback(FallbackTexture);

		// Fallback mesh: A unit box
		AssetCache<GPUMesh>& meshes = assets.Cache<GPUMesh>();
		FallbackMesh = meshes.Acquire(device, AssetKey::FromUri(ProceduralUriFor<BoxGenerator>()));
		meshes.SetFallback(FallbackMesh);

		KOI_LOG_DEBUG("Fallback mesh and texture registered");
	}

	void RendererResources::Shutdown(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		// Pipelines belong to the PipelineLibrary, which destroys them itself
		device.Destroy(WhiteTexture);
		device.Destroy(FlatNormalTexture);
		WhiteTexture          = {};
		FlatNormalTexture     = {};
		// The asset caches shut down first and have already released these
		FallbackMesh          = {};
		FallbackTexture       = {};
		TonemapPipeline       = {};
		MeshOpaquePipeline    = {};
		MeshWireframePipeline = {};
		GBufferPipeline       = {};
		LightingPipeline      = {};
		ShadowPipeline        = {};
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
			.VertexShader = "engine://Content/Shaders/TonemapVS",
			.PixelShader  = "engine://Content/Shaders/TonemapPS",
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
			.VertexShader = "engine://Content/Shaders/MeshVS",
			.PixelShader  = "engine://Content/Shaders/MeshPS",
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
			.VertexShader = "engine://Content/Shaders/MeshVS",
			.PixelShader  = "engine://Content/Shaders/MeshPS",
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
			.VertexShader = "engine://Content/Shaders/GBufferVS",
			.PixelShader  = "engine://Content/Shaders/GBufferPS",
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

		// --- Shadow map (depth-only: no pixel shader, no render targets) ---
		ShadowPipeline = library.Register(device, PipelineRecipe
		{
			.Name         = "ShadowMap",
			.VertexShader = "engine://Content/Shaders/ShadowVS",
			.Desc         =
			{
				// Raster-stage depth bias pushes fragments away from the light before the
				// depth test, which removes most self-shadowing acne for free
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_BACK, .FrontCounterClockwise = true, .DepthBias = 100, .SlopeScaledDepthBias = 2.0f },
				.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
				.RenderTargetCount   = 0,
				.DepthFormat         = Format::D32_FLOAT,
			},
		});

		// --- Deferred lighting (full-screen triangle: samples G-buffer -> HDR color) ---
		LightingPipeline = library.Register(device, PipelineRecipe
		{
			.Name         = "DeferredLighting",
			.VertexShader = "engine://Content/Shaders/LightingVS",
			.PixelShader  = "engine://Content/Shaders/LightingPS",
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
