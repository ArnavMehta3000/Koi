#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Resources/AssetRegistry.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	namespace
	{
		// Not real files: generated textures, keyed so nothing on disk can collide with them
		constexpr std::string_view kFallbackTextureUri = "koi://fallback/checker";
		constexpr std::string_view kWhiteTextureUri    = "koi://fallback/white";
		constexpr std::string_view kFallbackMeshPath   = "engine://Content/Meshes/Box.kmesh";

		D3D12_SAMPLER_DESC MakeLinearWrapDesc()
		{
			D3D12_SAMPLER_DESC d{};
			d.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			d.MaxLOD   = D3D12_FLOAT32_MAX;
			return d;
		}

		D3D12_SAMPLER_DESC MakeLinearClampDesc()
		{
			D3D12_SAMPLER_DESC d{};
			d.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			d.MaxLOD   = D3D12_FLOAT32_MAX;
			return d;
		}

		D3D12_SAMPLER_DESC MakePointClampDesc()
		{
			D3D12_SAMPLER_DESC d{};
			d.Filter   = D3D12_FILTER_MIN_MAG_MIP_POINT;
			d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			d.MaxLOD   = D3D12_FLOAT32_MAX;
			return d;
		}

		D3D12_SAMPLER_DESC MakeShadowCompareDesc()
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
	}

	void RendererResources::Init(GfxDevice& device)
	{
		LinearWrapSampler  = device.CreateSampler(MakeLinearWrapDesc());
		LinearClampSampler = device.CreateSampler(MakeLinearClampDesc());
		PointClampSampler  = device.CreateSampler(MakePointClampDesc());
		ShadowSampler      = device.CreateSampler(MakeShadowCompareDesc());

		KOI_LOG(Render, Debug, "Default samplers created");
	}

	void RendererResources::RegisterFallbackAssets(GfxDevice& device)
	{
		ZoneScoped;
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
		textures.Pin(WhiteTexture);

		const GPUTexture* whiteTexture = textures.Get(WhiteTexture);
		WhiteTextureSRV = whiteTexture ? whiteTexture->SRVIndex : INVALID_DESCRIPTOR;

		// Fallback mesh: the engine's unit box
		AssetCache<GPUMesh>& meshes = assets.Cache<GPUMesh>();
		FallbackMesh = meshes.Acquire(device, AssetKey(std::string(kFallbackMeshPath)));
		meshes.SetFallback(FallbackMesh);

		KOI_LOG(Render, Debug, "Fallback mesh and texture registered");
	}

	void RendererResources::Shutdown()
	{
		// Pinned in their caches, which shut down first and free them
		FallbackMesh          = {};
		FallbackTexture       = {};
		WhiteTexture          = {};
		WhiteTextureSRV       = INVALID_DESCRIPTOR;
		FallbackTextureSRV    = INVALID_DESCRIPTOR;
		// Samplers live in the sampler heap for the device lifetime; no explicit free needed.
	}
}
