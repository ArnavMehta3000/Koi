#include "Koi/Engine/Graphics/MeshGather.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"

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
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		out.Clear();

		// Loading a scene swaps the flecs world, which invalidates the cached query
		if (flecs::world_t* ecsWorld = world.GetECS().c_ptr(); m_queryWorld != ecsWorld)
		{
			m_queryWorld = ecsWorld;
			m_query      = world.GetECS().query<const Transform, const MeshRef>();
			KOI_LOG_DEBUG("MeshGatherer: rebuilt query for new world");
		}

		m_query.each([&](const Transform& xform, const MeshRef& meshRef)
		{
			const GPUMesh* mesh = world.GetMesh(meshRef.Handle);
			if (!mesh) return;

			const Matrix            worldMatrix = xform.ToMatrix();
			const DirectX::XMMATRIX worldXM     = DirectX::XMLoadFloat4x4(&worldMatrix);

			// The transform is only pushed once a submesh survives, so a fully culled entity
			// costs nothing in the object buffer
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

				out.Draws.push_back(MeshDrawItem{ .Mesh = mesh, .Draw = &draw, .ObjectIndex = objectIndex });
			}
		});
	}
}
