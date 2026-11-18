#pragma once
#include "Koi/Engine/Content/Shaders/Interop/Mesh.hlsli"
#include "Koi/Engine/Core/EnumFlags.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/ECS.h"
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Math/Math.h"
#include <span>
#include <vector>

namespace koi
{
	struct GPUMesh;
	struct GPUMeshDraw;
	struct RasterPipeline;

	enum class RenderFlag : u32
	{
		CastsShadow = 1u << 0,
	};
}
KOI_ENABLE_ENUM_FLAGS(koi::RenderFlag);

namespace koi
{
	using RenderFlags = EnumFlag<RenderFlag>;

	struct RenderObject
	{
		Entity         Owner;
		MeshHandle     Mesh;
		MaterialHandle Material;  // MaterialRef override, invalid when the mesh's own slots apply
		Matrix         World    = Matrix::Identity;
		RenderFlags    Flags    = RenderFlag::CastsShadow;
		u32            Layers   = 1;
	};

	struct RenderLight
	{
		Entity  Owner;
		Light   Data;
		Vector3 Position;
		Vector3 Direction;  // the entity's forward
	};

	// What a pass wants out of the scene
	struct GatherDesc
	{
		RenderFlags Require;
		RenderFlags Exclude;
		u32         Layers  = ~0u;
		bool        Normals = true;  // fill PerObjectData::NormalMatrix
		bool        Sort    = true;  // by pipeline, then mesh
	};

	// One submesh that survived culling
	struct MeshDrawItem
	{
		const GPUMesh*        Mesh;
		const GPUMeshDraw*    Draw;
		u32                   ObjectIndex;
		const RasterPipeline* Pipeline;      // null = the pass decides
		u32                   MaterialSlot;  // row in the MaterialTable
	};

	struct MeshGatherResult
	{
		std::vector<PerObjectData> Objects;
		std::vector<MeshDrawItem>  Draws;
		u32                        Culled = 0;  // submeshes rejected by the cull volume

		void Clear();
	};

	// Render-side mirror of a World: membership follows MeshRef / Light add and remove signals, poses and handles are copied by Sync
	class RenderScene : public NonCopyable
	{
	private:
		// Dense records keyed by entity index
		template<typename Record>
		class RecordSet
		{
		public:
			void Add(Entity e, Record record)
			{
				const u32 index = e.GetHandle().GetIndex();
				if (index >= m_slots.size())
					m_slots.resize(index + 1, kNone);
				if (m_slots[index] != kNone)
					return;

				m_slots[index] = static_cast<u32>(m_records.size());
				m_records.push_back(std::move(record));
			}

			void Remove(Entity e)
			{
				const u32 index = e.GetHandle().GetIndex();
				if (index >= m_slots.size() || m_slots[index] == kNone)
					return;

				const u32 slot = std::exchange(m_slots[index], kNone);
				if (slot != m_records.size() - 1)
				{
					m_records[slot] = std::move(m_records.back());
					m_slots[m_records[slot].Owner.GetHandle().GetIndex()] = slot;
				}
				m_records.pop_back();
			}

			[[nodiscard]] std::span<Record>       All() { return m_records; }
			[[nodiscard]] std::span<const Record> All() const { return m_records; }

		private:
			static constexpr u32 kNone = ~0u;

			std::vector<u32>    m_slots;
			std::vector<Record> m_records;
		};
	public:
		explicit RenderScene(ECS& ecs);

		void Sync();
		void Gather(const GatherDesc& desc, const CullVolume& volume, MeshGatherResult& out) const;

		[[nodiscard]] std::span<const RenderObject> GetObjects() const { return m_objects.All(); }
		[[nodiscard]] std::span<const RenderLight>  GetLights()  const { return m_lights.All(); }
		[[nodiscard]] const BoundingBox&            GetBounds()  const { return m_bounds; }

	private:
		RecordSet<RenderObject> m_objects;
		RecordSet<RenderLight>  m_lights;
		BoundingBox             m_bounds{ {}, {} };
	};
}
