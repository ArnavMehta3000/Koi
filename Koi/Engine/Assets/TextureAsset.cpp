#include "Koi/Engine/Assets/TextureAsset.h"
#include "Koi/Engine/Assets/TextureFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	std::unique_ptr<ImageAsset> AssetTraits<GPUTexture>::Load(const std::string& vfsPath)
	{
		KOI_ZONE_UNMUTED;  // Texture decode can arrive here from a parallel-for loop

		if (AssetFileTypeForPath(vfsPath) != AssetFileType::Texture)
		{
			KOI_LOG_ERROR("{} isn't a .ktex. Import the image in the editor and use what it writes", vfsPath);
			return nullptr;
		}

		std::optional<AssetFile> file = AssetFile::Load(vfsPath);
		if (!file) return nullptr;

		auto image      = std::make_unique<ImageAsset>();
		image->Prepared = ReadPreparedTexture(std::move(*file), vfsPath);
		if (!image->Prepared) return nullptr;

		image->Width  = static_cast<i32>(image->Prepared->Header->Width);
		image->Height = static_cast<i32>(image->Prepared->Header->Height);
		image->Name   = fs::path(vfsPath).filename().string();
		image->Path   = vfsPath;
		return image;
	}

	GPUTexture AssetTraits<GPUTexture>::Upload(GfxDevice& device, const ImageAsset& source)
	{
		return GPUTexture::Upload(device, source);
	}

	void AssetTraits<GPUTexture>::Destroy(GfxDevice& device, GPUTexture& resource)
	{
		resource.Release(device);
	}
}
