#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Assets/AssetTraits.h"

namespace koi
{
	class GfxDevice;
	struct ImageAsset;

	template<>
	struct AssetTraits<GPUTexture>
	{
		static constexpr std::string_view Name = "Texture";

		using Source = ImageAsset;

		// Reads a .ktex. Generated textures have no file, so they come in through AssetCache::AcquireFrom
		static std::unique_ptr<Source> Load(const std::string& vfsPath);
		static GPUTexture Upload(GfxDevice& device, const Source& source);
		static void Destroy(GfxDevice& device, GPUTexture& resource);
	};
}
