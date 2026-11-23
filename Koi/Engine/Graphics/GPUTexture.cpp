#include "Koi/Engine/Graphics/GPUTexture.h"
#include "Koi/Engine/Assets/AssetData.h"
#include "Koi/Engine/Assets/TextureFile.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/Device.h"

namespace koi
{
	namespace
	{
		GPUTexture UploadPrepared(GfxDevice& device, const ImageAsset& image)
		{
			const PreparedTexture& prepared = *image.Prepared;
			const TextureHeader& header     = *prepared.Header;

			GPUTexture out;
			out.Width  = header.Width;
			out.Height = header.Height;

			out.Handle = device.CreateTexture(TextureDesc
			{
				.Width        = header.Width,
				.Height       = header.Height,
				.DepthOrArray = header.ArraySize,
				.MipLevels    = header.MipLevels,
				.Format       = header.Format,
				.Usage        = TextureUsage::ShaderRead,
				.DebugName    = image.Name,
			});

			GfxDevice::UploadContext& upload = device.GetUploadContext();
			upload.Begin();
			upload.UploadTextureSubresources(out.Handle, prepared.Data, prepared.Subresources);
			upload.End();

			if (const Texture* tex = device.Get(out.Handle))
				out.SRVIndex = tex->SRVIndex;

			return out;
		}
	}

	GPUTexture GPUTexture::Upload(GfxDevice& device, const ImageAsset& image)
	{
		ZoneScoped;

		if (image.Prepared) return UploadPrepared(device, image);

		KOI_ASSERT(image.Width > 0 && image.Height > 0 && !image.Pixels.empty(), "GPUTexture::Upload: empty image");

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
		if (Handle) device.Destroy(Handle);

		Handle   = TextureHandle();
		SRVIndex = INVALID_DESCRIPTOR;
		Width    = 0;
		Height   = 0;
	}
}
