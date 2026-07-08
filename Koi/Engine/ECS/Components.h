#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Graphics/MeshRegistry.h"
#include <string>

namespace koi
{
	struct WorldTransform
	{
		math::float4x4 Value = math::float4x4::identity();
	};

	struct MeshRef
	{
		std::string SourcePath;  // VFS path of the GLTF this mesh was loaded from
		MeshHandle  Handle;      // lightweight handle into MeshRegistry
	};
}
