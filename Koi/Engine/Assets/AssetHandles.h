#pragma once
#include "Koi/Engine/Core/Handle.h"

namespace koi
{
	struct GPUMesh;
	struct GPUTexture;
	struct Material;

	template<typename T>
	using AssetHandle = Handle<T>;

	using MeshHandle       = AssetHandle<GPUMesh>;
	using GPUTextureHandle = AssetHandle<GPUTexture>;
	using MaterialHandle   = AssetHandle<Material>;
}
