#include "Koi/Engine/ECS/ComponentClone.h"
#include "Koi/Engine/Resources/AssetRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <utility>

namespace koi
{
	namespace
	{
		template<typename T>
		AssetCache<T>& Cache() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<T>(); }

		template<typename T>
		void Release(AssetHandle<T>& handle)
		{
			if (!handle) return;
			Cache<T>().Release(Engine::Get().GetSubsystem<Renderer>().GetDevice(), std::exchange(handle, {}));
		}
	}

	MeshRef ComponentClone<MeshRef>::Copy(const MeshRef& mesh)
	{
		return { Cache<GPUMesh>().AddRef(mesh.Handle) };
	}

	void ComponentClone<MeshRef>::Drop(MeshRef& mesh)
	{
		Release(mesh.Handle);
	}

	MaterialRef ComponentClone<MaterialRef>::Copy(const MaterialRef& material)
	{
		return { Cache<Material>().AddRef(material.Handle) };
	}

	void ComponentClone<MaterialRef>::Drop(MaterialRef& material)
	{
		Release(material.Handle);
	}
}
