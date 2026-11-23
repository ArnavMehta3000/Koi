#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Assets/AssetTraits.h"

namespace koi
{
	class GfxDevice;
	struct ModelAsset;

	template<>
	struct AssetTraits<GPUMesh>
	{
		static constexpr std::string_view Name = "Mesh";

		using Source = ModelAsset;

		static Result<std::unique_ptr<Source>> Load(const std::string& vfsPath);
		static GPUMesh Upload(GfxDevice& device, const Source& source);
		static void Destroy(GfxDevice& device, GPUMesh& resource);
	};

}
