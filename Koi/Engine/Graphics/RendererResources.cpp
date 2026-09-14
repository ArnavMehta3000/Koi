#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	// Not real files: generated textures, keyed so nothing on disk can collide with them
	constexpr std::string_view kFallbackTextureUri = "koi://fallback/checker";
	constexpr std::string_view kWhiteTextureUri    = "koi://fallback/white";
	constexpr std::string_view kFallbackMeshPath   = "engine://Content/Meshes/Box.gltf";

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
		LinearWrapSampler  = device.CreateSampler(MakeLinearWrapDesc());
		LinearClampSampler = device.CreateSampler(MakeLinearClampDesc());
		PointClampSampler  = device.CreateSampler(MakePointClampDesc());
		ShadowSampler      = device.CreateSampler(MakeShadowCompareDesc());

		KOI_LOG_DEBUG("Default samplers created");
		RegisterPipelines(device, library);
	}

	void RendererResources::RegisterFallbackAssets(GfxDevice& device)
	{
		// This function cannot be called in Init since it requires the AssetRegistry subsystem, which is constructed after the Renderer (and these resources)
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
		FallbackTexture = textures.AcquireFrom(device, AssetKey(std::string(kFallbackTextureUri)), checker);
		textures.SetFallback(FallbackTexture);

		// Every material row that binds no albedo samples this, so the shader never reads a null index
		const GPUTexture* checkerTexture = textures.Get(FallbackTexture);
		FallbackTextureSRV = checkerTexture ? checkerTexture->SRVIndex : INVALID_DESCRIPTOR;

		// White 1x1: what a material samples for a slot it leaves unbound
		ImageAsset white{ .Name = "White", .Path = std::string(kWhiteTextureUri), .Width = 1, .Height = 1, .Channels = 4 };
		white.Pixels.assign(4, koi::byte(0xFF));
		WhiteTexture = textures.AcquireFrom(device, AssetKey(std::string(kWhiteTextureUri)), white);

		const GPUTexture* whiteTexture = textures.Get(WhiteTexture);
		WhiteTextureSRV = whiteTexture ? whiteTexture->SRVIndex : INVALID_DESCRIPTOR;

		// Fallback mesh: the engine's unit box
		AssetCache<GPUMesh>& meshes = assets.Cache<GPUMesh>();
		FallbackMesh = meshes.Acquire(device, AssetKey(std::string(kFallbackMeshPath)));
		meshes.SetFallback(FallbackMesh);

		KOI_LOG_DEBUG("Fallback mesh and texture registered");
	}

	void RendererResources::Shutdown()
	{
		// Pipelines belong to the PipelineLibrary, which destroys them itself
		// The asset caches shut down first and have already released these
		FallbackMesh          = {};
		FallbackTexture       = {};
		WhiteTexture          = {};
		WhiteTextureSRV       = INVALID_DESCRIPTOR;
		FallbackTextureSRV    = INVALID_DESCRIPTOR;
		TonemapPipeline       = {};
		GBufferPipeline       = {};
		LightingPipeline      = {};
		ShadowPipeline        = {};
		// Samplers live in the sampler heap for the device lifetime; no explicit free needed.
	}

	void RendererResources::RegisterPipelines(GfxDevice& device, PipelineLibrary& library)
	{
		KOI_LOG_DEBUG("Registering default pipelines");

		// --- Tonemap ---
		TonemapPipeline = library.Register(device, PipelineRecipe::FullScreen(
			"Tonemap",
			"engine://Content/Shaders/TonemapVS",
			"engine://Content/Shaders/TonemapPS",
			Format::R8G8B8A8_UNORM));

		// The G-buffer pipeline is a material template, registered by the Renderer

		// Raster-stage depth bias pushes fragments away from the light before the depth test,
		// which removes most self-shadowing acne for free
		ShadowPipeline = library.Register(device, PipelineRecipe::DepthOnly(
			"ShadowMap",
			"engine://Content/Shaders/ShadowVS")
			.DepthBias(100, 2.0f));

		// --- Deferred lighting (full-screen triangle: samples G-buffer -> HDR color) ---
		LightingPipeline = library.Register(device, PipelineRecipe::FullScreen(
			"DeferredLighting",
			"engine://Content/Shaders/LightingVS",
			"engine://Content/Shaders/LightingPS",
			Format::R16G16B16A16_FLOAT));
	}
}
