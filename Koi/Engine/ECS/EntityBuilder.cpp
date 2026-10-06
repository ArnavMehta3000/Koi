#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	namespace
	{
		// Acquire before replace so a re-assignment of the same asset never drops to zero refs
		template<typename Ref, typename T>
		void SetAssetRef(Entity e, std::string_view virtualPath)
		{
			Engine& engine = Engine::Get();
			const AssetHandle<T> handle = engine.GetSubsystem<AssetRegistry>().Cache<T>()
				.Acquire(engine.GetSubsystem<Renderer>().GetDevice(), AssetKey(std::string(virtualPath)));
			if (!handle) return;

			e.remove<Ref>();  // the World's on_remove hook releases the previous handle
			e.set<Ref>({ handle });
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
		m_entity.set<Transform>(m_transform);
		return *this;
	}

	EntityBuilder& EntityBuilder::Rotated(const Quaternion& rotation)
	{
		m_transform.Rotation = rotation;
		m_entity.set<Transform>(m_transform);
		return *this;
	}

	EntityBuilder& EntityBuilder::Scaled(const Vector3& scale)
	{
		m_transform.Scale = scale;
		m_entity.set<Transform>(m_transform);
		return *this;
	}
}
