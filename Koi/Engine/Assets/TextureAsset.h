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

		// Decodes a standalone image file. Images embedded in a container have
		// no file of their own, so they are registered through AssetCache::AcquireFrom instead.
		static std::unique_ptr<Source> Load(const std::string& vfsPath);
		static GPUTexture Upload(GfxDevice& device, const Source& source);
		static void Destroy(GfxDevice& device, GPUTexture& resource);
	};

	static_assert(AssetType<GPUTexture>, "AssetTraits<GPUTexture> does not satisfy AssetType");
}
