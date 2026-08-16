#pragma once
#include "Koi/Engine/Assets/AssetCache.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Assets/TextureAsset.h"
#include "Koi/Engine/Runtime/Subsystems.h"
#include <tuple>

namespace koi
{
	class Renderer;

	// Holds one AssetCache per asset type
	template<AssetType... Ts>
	class AssetRegistryFor : public NonCopyable
	{
	public:
		using Dependencies = TypeList<Renderer>;  // Used by engine subsystems

		template<typename T> [[nodiscard]] AssetCache<T>&       Cache()       { return std::get<AssetCache<T>>(m_caches); }
		template<typename T> [[nodiscard]] const AssetCache<T>& Cache() const { return std::get<AssetCache<T>>(m_caches); }

		void Shutdown(GfxDevice& device)
		{
			(std::get<AssetCache<Ts>>(m_caches).ReleaseAll(device), ...);
		}

	private:
		std::tuple<AssetCache<Ts>...> m_caches;
	};

	using AssetRegistry = AssetRegistryFor<GPUMesh, GPUTexture>;
}
