#pragma once
#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Assets/AssetTraits.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <cassert>
#include <execution>
#include <memory>
#include <string>
#include <unordered_map>

namespace koi
{
	class GfxDevice;

	template<typename T>
	using AssetHandle = Handle<T>;

	// Owns every loaded asset of one type, de-duplicated by VFS path, ref-counted
	// Acquiring adds a reference, releasing drops one. Freeing the resource when the last ref goes
	template<AssetType T>
	class AssetCache : public NonCopyable
	{
	public:
		// Takes a reference, loading and uploading the asset if it isn't resident yet
		[[nodiscard]] AssetHandle<T> Acquire(GfxDevice& device, const std::string& vfsPath)
		{
			if (const AssetHandle<T> existing = AddRef(vfsPath))
			{
				KOI_LOG_TRACE("Reusing {} asset: {}", AssetTraits<T>::Name, vfsPath);
				return existing;
			}

			using Traits = AssetTraits<T>;

			const std::unique_ptr<Traits::Source> source = Traits::Load(vfsPath);
			if (!source)
			{
				KOI_LOG_ERROR("Failed to load {} asset: {}", Traits::Name, vfsPath);
				return {};
			}

			KOI_LOG_DEBUG("Loaded {} asset: {}", Traits::Name, vfsPath);
			return Insert(device, vfsPath, *source);
		}

		// Takes a reference on an asset supplied as an already-decoded source
		[[nodiscard]] AssetHandle<T> AcquireFrom(GfxDevice& device, const std::string& key, const typename AssetTraits<T>::Source& source)
		{
			if (const AssetHandle<T> existing = AddRef(key))
			{
				KOI_LOG_TRACE("Reusing embedded {} asset: {}", AssetTraits<T>::Name, key);
				return existing;
			}

			KOI_LOG_DEBUG("Loaded embedded {} asset: {}", AssetTraits<T>::Name, key);
			return Insert(device, key, source);
		}

		// Decodes a batch of paths concurrently, ahead of the Acquire loop that consumes them.
		// Results line up index-for-index with `paths`; an entry is null when the path is empty
		// (caller opted out), already resident, a duplicate of an earlier one, or failed to decode.
		[[nodiscard]] std::vector<std::unique_ptr<typename AssetTraits<T>::Source>>
		DecodeParallel(std::span<const std::string> paths) const
		{
			ZoneScoped;
			using Source = typename AssetTraits<T>::Source;

			std::vector<std::unique_ptr<Source>> decoded(paths.size());

			std::vector<u32> work;
			work.reserve(paths.size());
			for (u32 i = 0; i < paths.size(); ++i)
			{
				if (paths[i].empty() || m_pathToHandle.contains(paths[i])) continue;

				const bool duplicate = std::ranges::any_of(work,
					[&](u32 j) { return paths[j] == paths[i]; });

				if (!duplicate) work.push_back(i);
			}

			// A couple of decodes isn't worth the dispatch, the Acquire loop handles it inline
			if (work.size() < 2) return decoded;

			ZoneValue(work.size());

			std::for_each(std::execution::par, work.begin(), work.end(), [&](u32 i)
			{
				// par runs on system thread-pool threads
				const MuteZonesScope quiet;  // Mute tracy's thread-pool dump
				decoded[i] = AssetTraits<T>::Load(paths[i]);
			});

			return decoded;
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

		// Drops one reference, destroys the resource once the last one is gone
		void Release(GfxDevice& device, AssetHandle<T> handle)
		{
			Entry* entry = m_pool.Get(handle);
			if (!entry) return;

			assert(entry->RefCount > 0 && "Released an asset more times than it was acquired");
			if (--entry->RefCount > 0)
			{
				KOI_LOG_TRACE("Released {} asset: {} ({} ref(s) left)", AssetTraits<T>::Name, entry->Path, entry->RefCount);
				return;
			}

			KOI_LOG_DEBUG("Unloaded {} asset: {}", AssetTraits<T>::Name, entry->Path);

			m_pathToHandle.erase(entry->Path);
			AssetTraits<T>::Destroy(device, entry->Resource);
			m_pool.Erase(handle);
		}

		// Unconditional teardown at shutdown; ignores reference counts
		void ReleaseAll(GfxDevice& device)
		{
			u32 stillReferenced = 0;
			m_pool.ForEach([&](AssetHandle<T>, Entry& entry)
			{
				if (entry.RefCount > 0) ++stillReferenced;
				AssetTraits<T>::Destroy(device, entry.Resource);
			});

			// Expected until scene teardown releases what it acquired; useful once it does.
			if (stillReferenced > 0)
				KOI_LOG_DEBUG("{} {} asset(s) still referenced at shutdown", stillReferenced, AssetTraits<T>::Name);

			m_pool.Clear();  // bumps generations, so any outstanding handle now fails validation
			m_pathToHandle.clear();
		}

	private:
		// Returns the handle with an extra reference taken, or an invalid handle if not resident.
		AssetHandle<T> AddRef(const std::string& key)
		{
			auto it = m_pathToHandle.find(key);
			if (it == m_pathToHandle.end()) return {};

			if (Entry* entry = m_pool.Get(it->second))
				++entry->RefCount;

			return it->second;
		}

		AssetHandle<T> Insert(GfxDevice& device, const std::string& key, const typename AssetTraits<T>::Source& source)
		{
			const AssetHandle<T> handle = m_pool.Emplace(Entry
			{
				.Resource = AssetTraits<T>::Upload(device, source),
				.Path     = key,
				.RefCount = 1
			});
			m_pathToHandle.emplace(key, handle);
			return handle;
		}

	private:
		struct Entry
		{
			T           Resource;
			std::string Path;
			u32         RefCount = 0;
		};

		HandlePool<Entry, T>                            m_pool;
		std::unordered_map<std::string, AssetHandle<T>> m_pathToHandle;
	};
}
