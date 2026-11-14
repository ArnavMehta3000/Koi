#include "Koi/Engine/Graphics/MeshGather.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>

namespace koi
{
	void MeshGatherResult::Clear()
	{
		Objects.clear();
		Draws.clear();
		Culled = 0;
	}

	void MeshGatherer::Gather(World& world, const CullVolume& volume, bool needNormalMatrix, MeshGatherResult& out)
	{
		ZoneScoped;
		out.Clear();

		const AssetCache<Material>& materials = Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>();

		world.GetECS().Each<const Transform, const MeshRef>([&](Entity e, const Transform& xform, const MeshRef& meshRef)
		{
			const GPUMesh* mesh = world.GetMesh(meshRef.Handle);
			if (!mesh) return;

			// A MaterialRef on the entity wins over whatever the mesh's sidecar bound
			const MaterialRef* materialRef = e.TryGet<const MaterialRef>();
			const Material*    override    = materialRef ? materials.Get(materialRef->Handle) : nullptr;

			const Matrix            worldMatrix = xform.ToMatrix();
			const DirectX::XMMATRIX worldXM     = DirectX::XMLoadFloat4x4(&worldMatrix);

			// The transform is only pushed once a submesh survives, so a fully culled entity costs nothing in the object buffer
			u32 objectIndex = ~0u;

			for (const GPUMeshDraw& draw : mesh->DrawItems)
			{
				BoundingBox worldBounds;
				draw.Bounds.Transform(worldBounds, worldXM);

				if (!volume.Intersects(worldBounds))
				{
					++out.Culled;
					continue;
				}

				if (objectIndex == ~0u)
				{
					objectIndex = static_cast<u32>(out.Objects.size());
					out.Objects.push_back(PerObjectData
					{
						.World        = worldMatrix,
						.NormalMatrix = needNormalMatrix ? worldMatrix.Invert().Transpose() : Matrix::Identity,
					});
				}

				out.Draws.push_back(MeshDrawItem
				{
					.Mesh         = mesh,
					.Draw         = &draw,
					.ObjectIndex  = objectIndex,
					.Pipeline     = override ? override->Pipeline  : mesh->SlotPipelines[draw.MaterialSlot],
					.MaterialSlot = override ? override->TableSlot : mesh->TableSlots[draw.MaterialSlot],
				});
			}
		});

		// Sort by pipeline first (to set PSO), then mesh (to set IB)
		std::ranges::sort(out.Draws, [](const MeshDrawItem& a, const MeshDrawItem& b)
		{
			if (a.Pipeline != b.Pipeline)
				return std::less<>{}(a.Pipeline, b.Pipeline);

			return std::less<>{}(a.Mesh, b.Mesh);
		});
	}
}
