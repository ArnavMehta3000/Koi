#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Assets/AssetTraits.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <unordered_map>
#include <vector>

namespace koi
{
	class GfxDevice;

	// Owns every loaded asset of one type, de-duplicated by AssetKey, ref-counted
	// An asset whose last reference goes stays cached until Collect evicts it, so a quick re-acquire is free
	template<AssetType T>
	class AssetCache : public NonCopyable
	{
	public:
		// Takes a reference, loading and uploading the asset if it isn't resident yet
		[[nodiscard]] AssetHandle<T> Acquire(GfxDevice& device, const AssetKey& key)
		{
			if (const AssetHandle<T> existing = AddRef(key))
			{
				return existing;
			}

			using Traits = AssetTraits<T>;

			ZoneScopedN("Asset Load");
			ZoneText(key.Path().data(), key.Path().size());

			if (!key.IsValid())
			{
				KOI_LOG(Assets, Error, "Empty {} asset key", Traits::Name);
				return AcquireFallback();
			}

			const Result<std::unique_ptr<typename Traits::Source>> source = Traits::Load(key.Path());
			if (!source)
			{
				KOI_LOG(Assets, Error, "Couldn't load {} {}: {}", Traits::Name, key.Path(), source.error().Message);
				return AcquireFallback();
			}

			KOI_LOG(Assets, Debug, "Loaded {} asset: {}", Traits::Name, key.Path());
			return Insert(device, key, **source);
		}

		// Takes a reference on an asset that has no file, supplied as an already-built source
		[[nodiscard]] AssetHandle<T> AcquireFrom(GfxDevice& device, const AssetKey& key, const typename AssetTraits<T>::Source& source)
		{
			if (const AssetHandle<T> existing = AddRef(key))
			{
				return existing;
			}

			KOI_LOG(Assets, Debug, "Registered generated {} asset: {}", AssetTraits<T>::Name, key.ToString());
			return Insert(device, key, source);
		}


		[[nodiscard]] const T* Get(AssetHandle<T> handle) const
		{
			const Entry* entry = m_pool.Get(handle);
			return entry ? &entry->Resource : nullptr;
		}

		// Stands in for anything that fails to resolve or load. Pinned
		void SetFallback(AssetHandle<T> handle)
		{
			Pin(handle);
			m_fallback = handle;
		}

		// Never evict
		void Pin(AssetHandle<T> handle)
		{
			if (Entry* entry = m_pool.Get(handle))
			{
				entry->Pinned = true;
			}
		}

		// The resident asset under this key, or null
		[[nodiscard]] T* Find(const AssetKey& key)
		{
			const auto it = m_handleByKey.find(key);
			if (it == m_handleByKey.end()) return nullptr;

			Entry* entry = m_pool.Get(it->second);
			return entry ? &entry->Resource : nullptr;
		}

		// Loaded and cached, whether or not anything still references it
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
			return entry ? std::string_view(entry->Key.Path()) : std::string_view();
		}

		[[nodiscard]] AssetHandle<T> AddRef(AssetHandle<T> handle)
		{
			Entry* entry = m_pool.Get(handle);
			if (!entry) return {};

			++entry->RefCount;
			return handle;
		}

		// Drops one reference. The last one only marks the asset unused; Collect frees it later
		void Release(GfxDevice& /*device*/, AssetHandle<T> handle)
		{
			Entry* entry = m_pool.Get(handle);
			if (!entry) return;

			KOI_ASSERT(entry->RefCount > 0, "Released an asset more times than it was acquired");
			if (--entry->RefCount > 0)
			{
				return;
			}

			entry->UnusedSince = m_now;
			m_unused.push_back(handle);
		}

		// Frees every asset that has gone unreferenced for at least grace seconds
		void Collect(GfxDevice& device, f64 now, f64 grace)
		{
			m_now = now;
			std::erase_if(m_unused, [&](AssetHandle<T> handle)
			{
				const Entry* entry = m_pool.Get(handle);
				if (!entry || entry->RefCount > 0) return true;  // gone, or acquired again
				if (now - entry->UnusedSince < grace) return false;

				Evict(device, handle);
				return true;
			});
		}

		// Frees every unreferenced asset now, grace or not
		void EvictUnused(GfxDevice& device)
		{
			Collect(device, m_now, 0.0);
		}

		// Drops a stale copy
		void EvictIfUnused(GfxDevice& device, const AssetKey& key)
		{
			const auto it = m_handleByKey.find(key);
			if (it == m_handleByKey.end()) return;

			const Entry* entry = m_pool.Get(it->second);
			if (entry && entry->RefCount == 0)
			{
				Evict(device, it->second);
			}
		}

		[[nodiscard]] u32 GetRefCount(const AssetKey& key) const
		{
			const auto it = m_handleByKey.find(key);
			const Entry* entry = it != m_handleByKey.end() ? m_pool.Get(it->second) : nullptr;
			return entry ? entry->RefCount : 0;
		}

		// Unconditional teardown at shutdown, ignores reference counts
		void ReleaseAll(GfxDevice& device)
		{
			u32 stillReferenced = 0;
			m_pool.ForEach([&](AssetHandle<T>, Entry& entry)
			{
				if (entry.RefCount > (entry.Pinned ? 1u : 0u)) ++stillReferenced;
				AssetTraits<T>::Destroy(device, entry.Resource);
			});

			// Expected until scene teardown releases what it acquired; useful once it does.
			if (stillReferenced > 0)
			{
				KOI_LOG(Assets, Debug, "{} {} asset(s) still referenced at shutdown", stillReferenced, AssetTraits<T>::Name);
			}

			m_pool.Clear();  // bumps generations, so any outstanding handle now fails validation
			m_handleByKey.clear();
			m_unused.clear();
			m_fallback = {};
		}

	private:
		// Returns the handle with an extra reference taken, or an invalid handle if not resident.
		AssetHandle<T> AddRef(const AssetKey& key)
		{
			auto it = m_handleByKey.find(key);
			if (it == m_handleByKey.end()) return {};

			if (Entry* entry = m_pool.Get(it->second))
			{
				if (entry->RefCount++ == 0)
				{
					KOI_LOG(Assets, Trace, "Reused cached {} asset: {}", AssetTraits<T>::Name, key.ToString());
				}
			}

			return it->second;
		}

		void Evict(GfxDevice& device, AssetHandle<T> handle)
		{
			Entry* entry = m_pool.Get(handle);
			if (!entry) return;

			ZoneScopedN("Asset Evict");
			ZoneText(entry->Key.Path().data(), entry->Key.Path().size());

			KOI_LOG(Assets, Debug, "Unloaded {} asset: {}", AssetTraits<T>::Name, entry->Key.ToString());

			m_handleByKey.erase(entry->Key);
			AssetTraits<T>::Destroy(device, entry->Resource);
			m_pool.Erase(handle);
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
			u32      RefCount    = 0;
			f64      UnusedSince = 0.0;
			bool     Pinned      = false;
		};

		HandlePool<Entry, T>                          m_pool;
		std::unordered_map<AssetKey, AssetHandle<T>>  m_handleByKey;
		std::vector<AssetHandle<T>>                   m_unused;     // reached zero refs; Collect re-checks each
		AssetHandle<T>                                m_fallback{};
		f64                                           m_now = 0.0;  // the last Collect's time, stamped on release
	};
}
