#pragma once
#include "Koi/Engine/Assets/AssetCache.h"
#include "Koi/Engine/Graphics/GPUMesh.h"

namespace koi
{
	struct ModelAsset;

	template<>
	struct AssetTraits<GPUMesh>
	{
		static constexpr std::string_view Name = "Mesh";

		using Source = ModelAsset;

		static std::unique_ptr<Source> Load(const std::string& vfsPath);
		static GPUMesh Upload(GfxDevice& device, const Source& source);
		static void Destroy(GfxDevice& device, GPUMesh& resource);
	};

	static_assert(AssetType<GPUMesh>, "AssetTraits<GPUMesh> does not satisfy AssetType");

	using MeshHandle = AssetHandle<GPUMesh>;
}
