#include "Koi/Editor/Undo/ComponentOf.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi::editor
{
	namespace
	{
		template <typename T>
		AssetCache<T>& Cache() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<T>(); }

		template <typename Ref, typename T>
		std::optional<AssetKey> ReadRef(Entity e)
		{
			const Ref* ref = e.is_alive() ? e.try_get<Ref>() : nullptr;
			return ref ? std::optional<AssetKey>(Cache<T>().GetKey(ref->Handle)) : std::nullopt;
		}

		// Acquire before remove: if the new key is the one already held, the extra reference
		// keeps the asset resident across the removal that releases the old handle
		template <typename Ref, typename T>
		void WriteRef(Entity e, const std::optional<AssetKey>& key)
		{
			if (!e.is_alive()) return;

			if (!key)
			{
				e.remove<Ref>();  // World's on_remove hook releases the handle
				return;
			}

			const AssetHandle<T> handle = Cache<T>().Acquire(Engine::Get().GetSubsystem<Renderer>().GetDevice(), *key);
			if (!handle) return;

			e.remove<Ref>();
			e.set<Ref>({ handle });
		}
	}

	std::optional<AssetKey> ComponentOf<MeshRef>::Read() const                     { return ReadRef<MeshRef, GPUMesh>(Target);      }
	void ComponentOf<MeshRef>::Write(const std::optional<AssetKey>& key) const     { WriteRef<MeshRef, GPUMesh>(Target, key);       }
	std::optional<AssetKey> ComponentOf<MaterialRef>::Read() const                 { return ReadRef<MaterialRef, Material>(Target); }
	void ComponentOf<MaterialRef>::Write(const std::optional<AssetKey>& key) const { WriteRef<MaterialRef, Material>(Target, key);  }
}
