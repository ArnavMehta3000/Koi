#include "Koi/Engine/Resources/TextureAsset.h"
#include "Koi/Engine/Assets/AssetData.h"
#include "Koi/Engine/Assets/TextureFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/GPUTexture.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	static_assert(AssetType<GPUTexture>, "AssetTraits<GPUTexture> does not satisfy AssetType");

	Result<std::unique_ptr<ImageAsset>> AssetTraits<GPUTexture>::Load(const std::string& vfsPath)
	{
		KOI_ZONE_UNMUTED;  // Texture decode can arrive here from a parallel-for loop

		if (AssetFile::TypeForPath(vfsPath) != AssetFileType::Texture)
		{
			return Error::Make("it isn't a .ktex. Import the image in the editor and use what it writes");
		}

		Result<std::shared_ptr<const PreparedTexture>> prepared = AssetFile::Load(vfsPath).and_then(&PreparedTexture::Read);
		if (!prepared)
		{
			return std::unexpected(std::move(prepared.error()));
		}

		auto image      = std::make_unique<ImageAsset>();
		image->Prepared = std::move(*prepared);

		image->Width  = static_cast<i32>(image->Prepared->Header->Width);
		image->Height = static_cast<i32>(image->Prepared->Header->Height);
		image->Name   = fs::path(vfsPath).filename().string();
		image->Path   = vfsPath;
		return image;
	}

	GPUTexture AssetTraits<GPUTexture>::Upload(GfxDevice& device, const ImageAsset& source)
	{
		ZoneScoped;
		return GPUTexture::Upload(device, source);
	}

	void AssetTraits<GPUTexture>::Destroy(GfxDevice& device, GPUTexture& resource)
	{
		resource.Release(device);
	}
}
