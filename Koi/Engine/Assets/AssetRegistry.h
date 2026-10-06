#pragma once
#include "Koi/Engine/Assets/AssetCache.h"
#include "Koi/Engine/Assets/AssetIndex.h"
#include "Koi/Engine/Assets/MaterialAsset.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Assets/TextureAsset.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/GPUTexture.h"
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

		[[nodiscard]] AssetIndex&       Index()       { return m_index; }
		[[nodiscard]] const AssetIndex& Index() const { return m_index; }

		// The VFS is mounted before the engine exists, so the index can be built straight away
		void Init() { m_index.Rebuild(); }

		void Shutdown(GfxDevice& device)
		{
			(std::get<AssetCache<Ts>>(m_caches).ReleaseAll(device), ...);
		}

	private:
		std::tuple<AssetCache<Ts>...> m_caches;
		AssetIndex                    m_index;
	};

	// A class rather than an alias so it can be forward declared
	class AssetRegistry : public AssetRegistryFor<GPUMesh, GPUTexture, Material> {};
}
