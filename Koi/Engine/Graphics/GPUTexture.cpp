#include "Koi/Engine/Graphics/GPUTexture.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <cassert>

namespace koi
{
	GPUTexture GPUTexture::Upload(GfxDevice& device, const ImageAsset& image)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		assert(image.Width > 0 && image.Height > 0 && !image.Pixels.empty() && "GPUTexture::Upload: empty image");

		GPUTexture out;
		out.Width  = static_cast<u32>(image.Width);
		out.Height = static_cast<u32>(image.Height);

		out.Handle = device.CreateTexture(TextureDesc
		{
			.Width     = out.Width,
			.Height    = out.Height,
			.Format    = Format::R8G8B8A8_UNORM,
			.Usage     = TextureUsage::ShaderRead,
			.DebugName = image.Name.empty() ? "Mesh Texture" : std::string_view(image.Name),
		});

		GfxDevice::UploadContext& upload = device.GetUploadContext();
		upload.Begin();
		upload.UploadTexture(out.Handle, image.Pixels.data(), image.Pixels.size());
		upload.End();

		if (const Texture* tex = device.Get(out.Handle))
			out.SRVIndex = tex->SRVIndex;

		return out;
	}

	void GPUTexture::Release(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		if (Handle) device.Destroy(Handle);

		Handle   = TextureHandle();
		SRVIndex = INVALID_DESCRIPTOR;
		Width    = 0;
		Height   = 0;
	}
}
