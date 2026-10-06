#pragma once
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Math/Math.h"
#include <vector>

namespace koi
{
	class  World;
	struct GPUMesh;
	struct GPUMeshDraw;

	// One submesh that survived culling, plus where its transform landed in the object buffer
	struct MeshDrawItem
	{
		const GPUMesh*     Mesh;
		const GPUMeshDraw* Draw;
		u32                ObjectIndex;
		PipelineHandle     Pipeline; 
		u32                MaterialSlot; // row in the MaterialTable
	};

	struct MeshGatherResult
	{
		std::vector<PerObjectData> Objects;
		std::vector<MeshDrawItem>  Draws;
		u32                        Culled = 0;  // submeshes rejected by the cull volume

		void Clear();
	};

	// Walks (Transform, MeshRef) and builds a culled, flat draw list. Shared by every pass
	// that draws the scene; the cull volume is what makes each pass's result different
	class MeshGatherer
	{
	public:
		// NormalMatrix is only computed when the consumer needs it - a depth-only pass does not
		void Gather(World& world, const CullVolume& volume, bool needNormalMatrix, MeshGatherResult& out);
	};
}
