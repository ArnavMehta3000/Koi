#include "Koi/Engine/Assets/TextureAsset.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "stb_image.h"

namespace koi
{
	std::unique_ptr<ImageAsset> AssetTraits<GPUTexture>::Load(const std::string& vfsPath)
	{
		KOI_ZONE_UNMUTED;  // Texture decode can arrive here from a parallel-for loop

		if (vfsPath.contains('#'))
		{
			KOI_LOG_ERROR("Texture '{}' is embedded in a container and cannot be loaded by path", vfsPath);
			return nullptr;
		}

		// Keys are VFS paths, glTF-sourced or otherwise. The fallback covers a raw disk path
		const fs::path diskPath =
			VirtualFileSystem::Get().TryResolveToDisk(vfsPath).value_or(fs::path(vfsPath));

		auto image = std::make_unique<ImageAsset>();

		stbi_uc* pixels = stbi_load(diskPath.string().c_str(), &image->Width, &image->Height, &image->Channels, 4);
		if (!pixels)
		{
			KOI_LOG_WARN("Failed to decode texture '{}': {}", diskPath.string(), stbi_failure_reason());
			return nullptr;
		}

		// stbi was asked for 4 components, so the data is RGBA regardless of the file's channels.
		image->Channels = 4;
		const size_t byteCount = static_cast<size_t>(image->Width) * image->Height * 4;
		image->Pixels.assign(pixels, pixels + byteCount);
		stbi_image_free(pixels);

		image->Name = diskPath.filename().string();
		image->Path = vfsPath;

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
