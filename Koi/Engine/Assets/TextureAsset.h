#pragma once
#include "Koi/Engine/Assets/AssetCache.h"
#include "Koi/Engine/Graphics/GPUTexture.h"

namespace koi
{
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

	static_assert(AssetType<GPUTexture>, "AssetTraits<GPUTexture> does not satisfy AssetType");
}
