#pragma once
#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Assets/AssetIdentity.h"
#include "Koi/Engine/Assets/AssetTraits.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <cassert>
#include <execution>

namespace koi
{
	class GfxDevice;

	template<typename T>
	using AssetHandle = Handle<T>;

	// Owns every loaded asset of one type, de-duplicated by AssetKey, ref-counted
	// Acquiring adds a reference, releasing drops one. Freeing the resource when the last ref goes
	template<AssetType T>
	class AssetCache : public NonCopyable
	{
	public:
		// Takes a reference, loading and uploading the asset if it isn't resident yet
		[[nodiscard]] AssetHandle<T> Acquire(GfxDevice& device, const AssetKey& key)
		{
			if (const AssetHandle<T> existing = AddRef(key))
			{
				KOI_LOG_TRACE("Reusing {} asset: {}", AssetTraits<T>::Name, key.ToString());
				return existing;
			}

			using Traits = AssetTraits<T>;

			const std::string vfsPath(AssetIdentity::Get().PathFor(key));
			if (vfsPath.empty())
			{
				KOI_LOG_ERROR("Unresolved {} asset key: {}", Traits::Name, key.ToString());
				return AcquireFallback();
			}

			const std::unique_ptr<Traits::Source> source = Traits::Load(vfsPath);
			if (!source)
			{
				KOI_LOG_ERROR("Failed to load {} asset: {}", Traits::Name, vfsPath);
				return AcquireFallback();
			}

			KOI_LOG_DEBUG("Loaded {} asset: {}", Traits::Name, vfsPath);
			return Insert(device, key, *source);
		}

		// Takes a reference on an asset supplied as an already-decoded source
		[[nodiscard]] AssetHandle<T> AcquireFrom(GfxDevice& device, const AssetKey& key, const typename AssetTraits<T>::Source& source)
		{
			if (const AssetHandle<T> existing = AddRef(key))
			{
				KOI_LOG_TRACE("Reusing embedded {} asset: {}", AssetTraits<T>::Name, key.ToString());
				return existing;
			}

			KOI_LOG_DEBUG("Loaded embedded {} asset: {}", AssetTraits<T>::Name, key.ToString());
			return Insert(device, key, source);
		}

		// Decodes a batch of keys concurrently, ahead of the Acquire loop that consumes them.
		// Results line up index-for-index with `keys`; an entry is null when the key is invalid
		// (caller opted out), already resident, a duplicate of an earlier one, or failed to decode.
		[[nodiscard]] std::vector<std::unique_ptr<typename AssetTraits<T>::Source>>
		DecodeParallel(std::span<const AssetKey> keys) const
		{
			ZoneScoped;
			using Source = typename AssetTraits<T>::Source;

			std::vector<std::unique_ptr<Source>> decoded(keys.size());

			std::vector<u32> work;
			work.reserve(keys.size());
			for (u32 i = 0; i < keys.size(); ++i)
			{
				if (!keys[i].IsValid() || m_handleByKey.contains(keys[i])) continue;

				const bool duplicate = std::ranges::any_of(work, [&](u32 j) { return keys[j] == keys[i]; });

				if (!duplicate) work.push_back(i);
			}

			// A couple of decodes isn't worth the dispatch, the Acquire loop handles it inline
			if (work.size() < 2) return decoded;

			ZoneValue(work.size());

			// AssetIdentity is read-only here
			std::vector<std::string> paths(keys.size());
			for (const u32 i : work)
			{
				paths[i] = std::string(AssetIdentity::Get().PathFor(keys[i]));
			}

			std::for_each(std::execution::par, work.begin(), work.end(), [&](u32 i)
			{
				// par runs on system thread-pool threads
				const MuteZonesScope quiet;  // Mute tracy's thread-pool dump
				if (!paths[i].empty()) decoded[i] = AssetTraits<T>::Load(paths[i]);
			});

			return decoded;
		}

		[[nodiscard]] const T* Get(AssetHandle<T> handle) const
		{
			const Entry* entry = m_pool.Get(handle);
			return entry ? &entry->Resource : nullptr;
		}

		// Stands in for anything that fails to resolve or load
		void SetFallback(AssetHandle<T> handle) { m_fallback = handle; }

		// Whether anything currently holds this key resident
		[[nodiscard]] bool IsResident(const AssetKey& key) const { return m_handleByKey.contains(key); }

		// The key this asset is held under; invalid for an invalid handle
		[[nodiscard]] AssetKey GetKey(AssetHandle<T> handle) const
		{
			const Entry* entry = m_pool.Get(handle);
			return entry ? entry->Key : AssetKey();
		}

		[[nodiscard]] std::string_view GetPath(AssetHandle<T> handle) const
		{
			const Entry* entry = m_pool.Get(handle);
			return entry ? AssetIdentity::Get().PathFor(entry->Key) : std::string_view();
		}

		// Drops one reference, destroys the resource once the last one is gone
		void Release(GfxDevice& device, AssetHandle<T> handle)
		{
			Entry* entry = m_pool.Get(handle);
			if (!entry) return;

			assert(entry->RefCount > 0 && "Released an asset more times than it was acquired");
			if (--entry->RefCount > 0)
			{
				KOI_LOG_TRACE("Released {} asset: {} ({} ref(s) left)", AssetTraits<T>::Name, entry->Key.ToString(), entry->RefCount);
				return;
			}

			KOI_LOG_DEBUG("Unloaded {} asset: {}", AssetTraits<T>::Name, entry->Key.ToString());

			m_handleByKey.erase(entry->Key);
			AssetTraits<T>::Destroy(device, entry->Resource);
			m_pool.Erase(handle);
		}

		// Unconditional teardown at shutdown, ignores reference counts
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
			{
				KOI_LOG_DEBUG("{} {} asset(s) still referenced at shutdown", stillReferenced, AssetTraits<T>::Name);
			}

			m_pool.Clear();  // bumps generations, so any outstanding handle now fails validation
			m_handleByKey.clear();
			m_fallback = {};
		}

	private:
		// Returns the handle with an extra reference taken, or an invalid handle if not resident.
		AssetHandle<T> AddRef(const AssetKey& key)
		{
			auto it = m_handleByKey.find(key);
			if (it == m_handleByKey.end()) return {};

			if (Entry* entry = m_pool.Get(it->second))
				++entry->RefCount;

			return it->second;
		}

		// The caller releases what it gets back either way, so the fallback is handed out
		// with a reference of its own rather than shared off the one the cache holds
		AssetHandle<T> AcquireFallback()
		{
			Entry* entry = m_pool.Get(m_fallback);
			if (!entry) return {};

			++entry->RefCount;
			return m_fallback;
		}

		AssetHandle<T> Insert(GfxDevice& device, const AssetKey& key, const typename AssetTraits<T>::Source& source)
		{
			const AssetHandle<T> handle = m_pool.Emplace(Entry
			{
				.Resource = AssetTraits<T>::Upload(device, source),
				.Key      = key,
				.RefCount = 1
			});
			m_handleByKey.emplace(key, handle);
			return handle;
		}

	private:
		struct Entry
		{
			T        Resource;
			AssetKey Key;
			u32      RefCount = 0;
		};

		HandlePool<Entry, T>                          m_pool;
		std::unordered_map<AssetKey, AssetHandle<T>>  m_handleByKey;
		AssetHandle<T>                                m_fallback{};
	};
}
