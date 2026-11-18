#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <utility>

namespace koi
{
	namespace
	{
		// Acquire before replace so a re-assignment of the same asset never drops to zero refs
		template<typename Ref, typename T>
		void SetAssetRef(Entity e, std::string_view virtualPath)
		{
			Engine& engine = Engine::Get();
			GfxDevice& device = engine.GetSubsystem<Renderer>().GetDevice();
			auto& cache = engine.GetSubsystem<AssetRegistry>().Cache<T>();
			const AssetHandle<T> handle = cache.Acquire(device, AssetKey(std::string(virtualPath)));
			if (!handle) return;

			// Swapped in place: a deferred Remove inside a query would land after the Set and drop the new handle
			if (Ref* existing = e.TryGet<Ref>())
			{
				const AssetHandle<T> previous = std::exchange(existing->Handle, handle);
				if (previous) cache.Release(device, previous);
				return;
			}
			e.Set<Ref>({ handle });
		}
	}

	EntityBuilder& EntityBuilder::WithMesh(std::string_view virtualPath)
	{
		SetAssetRef<MeshRef, GPUMesh>(m_entity, virtualPath);
		return *this;
	}

	EntityBuilder& EntityBuilder::WithMaterial(std::string_view virtualPath)
	{
		SetAssetRef<MaterialRef, Material>(m_entity, virtualPath);
		return *this;
	}

	EntityBuilder& EntityBuilder::At(const Vector3& position)
	{
		m_transform.Position = position;
		m_entity.Set<Transform>(m_transform);
		return *this;
	}

	EntityBuilder& EntityBuilder::Rotated(const Quaternion& rotation)
	{
		m_transform.Rotation = rotation;
		m_entity.Set<Transform>(m_transform);
		return *this;
	}

	EntityBuilder& EntityBuilder::Scaled(const Vector3& scale)
	{
		m_transform.Scale = scale;
		m_entity.Set<Transform>(m_transform);
		return *this;
	}
}
