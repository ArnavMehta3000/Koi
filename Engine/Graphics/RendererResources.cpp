#include "Graphics/RendererResources.h"
#include "Graphics/RHI/Device.h"
#include "Memory/MemoryTracker.h"

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
	}

	void RendererResources::Shutdown(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		device.Destroy(WhiteTexture);
		device.Destroy(FlatNormalTexture);
		WhiteTexture      = {};
		FlatNormalTexture = {};
		// Samplers live in the sampler heap for the device lifetime; no explicit free needed.
	}
}
