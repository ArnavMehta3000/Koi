#pragma once
#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Assets/AssetTraits.h"
#include "Koi/Engine/Logging/Logger.h"
#include <unordered_map>

namespace koi
{
	class GfxDevice;

	template<typename T>
	using AssetHandle = Handle<T>;

	// Owns every loaded asset of one type, de-duplicated by VFS path
	template<AssetType T>
	class AssetCache : public NonCopyable
	{
	public:
		// Returns the existing handle when `vfsPath` is already loaded, otherwise loads and uploads it
		[[nodiscard]] AssetHandle<T> Load(GfxDevice& device, const std::string& vfsPath)
		{
			if (auto it = m_pathToHandle.find(vfsPath); it != m_pathToHandle.end())
				return it->second;

			using Traits = AssetTraits<T>;

			const std::unique_ptr<Traits::Source> source = Traits::Load(vfsPath);
			if (!source)
			{
				KOI_LOG_ERROR("Failed to load {} asset: {}", Traits::Name, vfsPath);
				return {};
			}

			const AssetHandle<T> handle = m_pool.Emplace(Entry
			{
				.Resource = Traits::Upload(device, *source),
				.Path = vfsPath
			});
			m_pathToHandle.emplace(vfsPath, handle);

			KOI_LOG_DEBUG("Loaded {} asset: {}", Traits::Name, vfsPath);
			return handle;
		}

		[[nodiscard]] const T* Get(AssetHandle<T> handle) const
		{
			const Entry* entry = m_pool.Get(handle);
			return entry ? &entry->Resource : nullptr;
		}

		// The VFS path this asset was loaded from; empty for an invalid handle
		[[nodiscard]] std::string_view GetPath(AssetHandle<T> handle) const
		{
			const Entry* entry = m_pool.Get(handle);
			return entry ? std::string_view(entry->Path) : std::string_view();
		}

		void Release(GfxDevice& device, AssetHandle<T> handle)
		{
			Entry* entry = m_pool.Get(handle);
			if (!entry) return;

			m_pathToHandle.erase(entry->Path);
			AssetTraits<T>::Destroy(device, entry->Resource);
			m_pool.Erase(handle);
		}

		void ReleaseAll(GfxDevice& device)
		{
			m_pool.ForEach([&device](AssetHandle<T>, Entry& entry) { AssetTraits<T>::Destroy(device, entry.Resource); });
			m_pool.Clear();  // bumps generations, so any outstanding handle now fails validation
			m_pathToHandle.clear();
		}

	private:
		struct Entry
		{
			T           Resource;
			std::string Path;
		};

		HandlePool<Entry, T>                            m_pool;
		std::unordered_map<std::string, AssetHandle<T>> m_pathToHandle;
	};
}
