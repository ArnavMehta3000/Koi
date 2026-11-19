#include "Koi/Engine/Graphics/RenderScene.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <utility>

namespace koi
{
	void MeshGatherResult::Clear()
	{
		Objects.clear();
		Draws.clear();
		Culled = 0;
	}

	RenderScene::RenderScene(ECS& ecs)
		: m_ecs(&ecs)
	{
		// Add ECS hooks
		ecs.OnAdd<MeshRef>([this](Entity e, MeshRef&)     { m_objects.Add(e, RenderObject{ .Owner = e }); });
		ecs.OnRemove<MeshRef>([this](Entity e, MeshRef&)  { m_objects.Remove(e); });

		ecs.OnAdd<Light>([this](Entity e, Light&)         { m_lights.Add(e, RenderLight{ .Owner = e }); });
		ecs.OnRemove<Light>([this](Entity e, Light&)      { m_lights.Remove(e); });
	}

	void RenderScene::Sync()
	{
		KOI_SCOPE("RenderScene Sync");

		const AssetCache<GPUMesh>& meshes = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();
		m_bounds = BoundingBox{ {}, {} };

		m_postProcess = PostProcess{};
		bool foundPost = false;
		m_ecs->Each<const PostProcess>([&](const PostProcess& post)
		{
			if (!std::exchange(foundPost, true)) m_postProcess = post;
		});

		for (RenderObject& object : m_objects.All())
		{
			const Entity e = object.Owner;
			object.Mesh = e.Get<const MeshRef>().Handle;

			const MaterialRef* material = e.TryGet<const MaterialRef>();
			object.Material = material ? material->Handle : MaterialHandle{};

			// No WorldTransform until the first propagation; draw it at the origin until then
			const WorldTransform* world = e.TryGet<const WorldTransform>();
			object.World = world ? world->World : Matrix::Identity;

			const GPUMesh* mesh = meshes.Get(object.Mesh);
			if (mesh && !math::IsSingular(object.World))
			{
				BoundingBox worldBounds;
				mesh->Bounds.Transform(worldBounds, object.World);
				math::MergeBounds(m_bounds, worldBounds);
			}
		}

		for (RenderLight& light : m_lights.All())
		{
			const Entity e = light.Owner;
			light.Data = e.Get<const Light>();

			const WorldTransform* world = e.TryGet<const WorldTransform>();
			light.Position  = world ? world->Position : Vector3::Zero;
			light.Direction = Matrix::CreateFromQuaternion(world ? world->Rotation : Quaternion::Identity).Forward();
			light.Direction.Normalize();
		}

		FrameStats::Count("Render Objects", static_cast<i64>(m_objects.All().size()));
		FrameStats::Count("Render Lights",  static_cast<i64>(m_lights.All().size()));
	}

	void RenderScene::Gather(const GatherDesc& desc, const CullVolume& volume, MeshGatherResult& out) const
	{
		ZoneScoped;
		out.Clear();

		AssetRegistry&              assets    = Engine::Get().GetSubsystem<AssetRegistry>();
		const AssetCache<GPUMesh>&  meshes    = assets.Cache<GPUMesh>();
		const AssetCache<Material>& materials = assets.Cache<Material>();

		for (const RenderObject& object : m_objects.All())
		{
			if (!object.Flags.HasAll(desc.Require) || object.Flags.HasAny(desc.Exclude)) continue;
			if ((object.Layers & desc.Layers) == 0) continue;

			const GPUMesh* mesh = meshes.Get(object.Mesh);
			if (!mesh) continue;

			// Zero scale draws nothing
			if (math::IsSingular(object.World)) continue;

			// A MaterialRef on the entity wins over whatever the mesh's sidecar bound
			const Material* override = materials.Get(object.Material);

			// The transform is only pushed once a submesh survives, so a fully culled entity costs nothing in the object buffer
			u32 objectIndex = ~0u;

			for (const GPUMeshDraw& draw : mesh->DrawItems)
			{
				BoundingBox worldBounds;
				draw.Bounds.Transform(worldBounds, object.World);

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
						.World        = object.World,
						.NormalMatrix = desc.Normals ? object.World.Invert().Transpose() : Matrix::Identity,
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
		}

		if (!desc.Sort) return;

		// Sort by pipeline first (to set PSO), then mesh (to set IB)
		std::ranges::sort(out.Draws, [](const MeshDrawItem& a, const MeshDrawItem& b)
		{
			if (a.Pipeline != b.Pipeline)
				return std::less<>{}(a.Pipeline, b.Pipeline);

			return std::less<>{}(a.Mesh, b.Mesh);
		});
	}
}
