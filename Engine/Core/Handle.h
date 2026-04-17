#include "Core/Types.h"
#include "Core/Assert.h"
#include <compare>
#include <limits>
#include <vector>
#include <array>
#include <span>

namespace koi
{
	template <typename T, size_t IndexBits = 20, size_t GenerationBits = 12>
	class Handle
	{
		static_assert(IndexBits + GenerationBits <= 64, "IndexBits + GenerationBits must be <= 64");

	public:
		using IndexType = std::conditional_t<(IndexBits + GenerationBits <= 32), u32, u64>;

		static constexpr IndexType INDEX_MASK      = IndexBits      == 64 ? ~IndexType(0) : (IndexType(1) << IndexBits)      - 1;
		static constexpr IndexType GENERATION_MASK = GenerationBits == 64 ? ~IndexType(0) : (IndexType(1) << GenerationBits) - 1;
		static constexpr IndexType INVALID_ID      = std::numeric_limits<IndexType>::max();

		constexpr Handle() noexcept = default;
		constexpr Handle(IndexType index, IndexType generation) noexcept
			: m_id((index& INDEX_MASK) | ((generation & GENERATION_MASK) << IndexBits)) { }

		auto operator<=>(const Handle&) const = default;

		[[nodiscard]] constexpr inline IndexType GetIndex()      const noexcept { return m_id & INDEX_MASK; }
		[[nodiscard]] constexpr inline IndexType GetGeneration() const noexcept { return (m_id >> IndexBits) & GENERATION_MASK; }
		[[nodiscard]] constexpr IndexType        GetId()         const noexcept { return m_id; }
		[[nodiscard]] constexpr bool             IsValid()       const noexcept { return m_id != INVALID_ID; }

		constexpr explicit operator bool() const noexcept { return IsValid(); }

	private:
		IndexType m_id = INVALID_ID;
	};

	
	template <typename T, typename Tag = T, size_t IndexBits = 20, size_t GenBits = 12>
	class HandlePool
	{
	public:
		using HandleType = Handle<T, IndexBits, GenBits>;
		using IndexType = typename HandleType::IndexType;

		HandlePool() = default;

		[[nodiscard]] u64                       GetSize() const noexcept { return m_dense.size(); }
		[[nodiscard]] inline std::span<T>       GetData() noexcept       { return m_dense;        }
		[[nodiscard]] inline std::span<const T> GetData() const noexcept { return m_dense;        }

		[[nodiscard]] T* Get(HandleType handle) noexcept
		{
			return IsValid(handle) ? &m_dense[m_sparse[handle.GetIndex()].NextFree] : nullptr;
		}

		[[nodiscard]] const T* Get(HandleType handle) const noexcept
		{
			return IsValid(handle) ? &m_dense[m_sparse[handle.GetIndex()].NextFree] : nullptr;
		}

		void Clear()
		{
			m_dense.clear();
			m_denseToSparse.clear();
			m_sparse.clear();
			m_freeListHead = INVALID_IDX;
		}

		[[nodiscard]] bool IsValid(HandleType handle) const noexcept
		{
			if (!handle)
				return false;

			IndexType index = handle.GetIndex();
			if (index >= static_cast<IndexType>(m_sparse.size()))
				return false;

			return m_sparse[index].Generation == handle.GetGeneration();
		}

		template <typename... Args>
		[[nodiscard]] HandleType Emplace(Args&&... args)
		{
			IndexType sparseIdx;
			IndexType generation;

			if (m_freeListHead != INVALID_IDX)
			{
				sparseIdx = m_freeListHead;
				m_freeListHead = m_sparse[sparseIdx].NextFree;
				generation = m_sparse[sparseIdx].Generation;
			}
			else
			{
				sparseIdx = static_cast<IndexType>(m_sparse.size());
				KOI_ASSERT_MSG(sparseIdx <= HandleType::INDEX_MASK, "Too many handles in pool!");
				generation = 0;
				m_sparse.push_back({ 0, generation });
			}

			IndexType denseIdx = static_cast<IndexType>(m_dense.size());
			m_sparse[sparseIdx].NextFree = denseIdx;
			m_dense.emplace_back(std::forward<Args>(args)...);
			m_denseToSparse.push_back(sparseIdx);

			return HandleType(sparseIdx, generation);
		}

		void Erase(HandleType handle)
		{
			if (!IsValid(handle)) return;

			IndexType sparseIdx    = handle.GetIndex();
			IndexType denseIdx     = m_sparse[sparseIdx].NextFree;
			IndexType lastDenseIdx = static_cast<IndexType>(m_dense.size()) - 1;

			if (denseIdx != lastDenseIdx)
			{
				m_dense[denseIdx] = std::move(m_dense[lastDenseIdx]);

				IndexType lastSparseIdx          = m_denseToSparse[lastDenseIdx];
				m_sparse[lastSparseIdx].NextFree = denseIdx;
				m_denseToSparse[denseIdx]        = lastSparseIdx;
			}

			m_dense.pop_back();
			m_denseToSparse.pop_back();

			m_sparse[sparseIdx].Generation++;
			m_sparse[sparseIdx].NextFree = m_freeListHead;
			m_freeListHead = sparseIdx;
		}

	private:
		static constexpr IndexType INVALID_IDX = std::numeric_limits<IndexType>::max();

		struct Slot
		{
			IndexType NextFree;   // Also dense index when alive
			IndexType Generation;
		};

		std::vector<Slot>      m_sparse;
		std::vector<IndexType> m_denseToSparse;
		std::vector<T>         m_dense;
		IndexType              m_freeListHead = INVALID_IDX;
	};


	template <typename T, size_t Capacity, typename Tag = T, size_t IndexBits = 20, size_t GenBits = 12>
	class StaticHandlePool
	{
		static_assert(Capacity <= (size_t(1) << IndexBits), "Capacity exceeds IndexBits range");

	public:
		using HandleType = Handle<T, IndexBits, GenBits>;
		using IndexType = typename HandleType::IndexType;

		StaticHandlePool()
		{
			// Build the initial free list through all sparse slots
			for (size_t i = 0; i < Capacity; ++i)
			{
				m_sparse[i].NextFree = static_cast<IndexType>(i + 1);
				m_sparse[i].Generation = 0;
			}
			m_sparse[Capacity - 1].NextFree = INVALID_IDX;
			m_freeListHead = 0;
		}

		[[nodiscard]] u64                  GetSize()  const noexcept { return m_size;                              }
		[[nodiscard]] static constexpr u64 GetCapacity() noexcept    { return Capacity;                            }
		[[nodiscard]] std::span<T>         GetData() noexcept        { return std::span<T>(m_dense, m_size);       }
		[[nodiscard]] std::span<const T>   GetData() const noexcept  { return std::span<const T>(m_dense, m_size); }

		[[nodiscard]] T* Get(HandleType handle) noexcept
		{
			if (!IsValid(handle)) return nullptr;
			return &m_dense[m_sparse[handle.GetIndex()].NextFree];
		}

		[[nodiscard]] const T* Get(HandleType handle) const noexcept
		{
			if (!IsValid(handle)) return nullptr;
			return &m_dense[m_sparse[handle.GetIndex()].NextFree];
		}

		void Clear()
		{
			// Destroy live objects and rebuild the free list
			for (size_t i = 0; i < m_size; ++i)
				m_dense[i].~T();

			m_size = 0;

			for (size_t i = 0; i < Capacity; ++i)
			{
				m_sparse[i].NextFree = static_cast<IndexType>(i + 1);
				m_sparse[i].Generation++;  // Invalidate all outstanding handles
			}
			m_sparse[Capacity - 1].NextFree = INVALID_IDX;
			m_freeListHead = 0;
		}

		[[nodiscard]] bool IsValid(HandleType handle) const noexcept
		{
			if (!handle)
				return false;

			IndexType index = handle.GetIndex();
			if (index >= static_cast<IndexType>(Capacity))
				return false;

			return m_sparse[index].Generation == handle.GetGeneration();
		}

		template <typename... Args>
		[[nodiscard]] HandleType Emplace(Args&&... args)
		{
			KOI_ASSERT_MSG(m_freeListHead != INVALID_IDX, "StaticHandlePool is full, increase capacity!");

			IndexType sparseIdx  = m_freeListHead;
			m_freeListHead       = m_sparse[sparseIdx].NextFree;
			IndexType generation = m_sparse[sparseIdx].Generation;

			IndexType denseIdx = static_cast<IndexType>(m_size);
			m_sparse[sparseIdx].NextFree = denseIdx;

			// Placement-new into the dense storage
			new (&m_dense[m_size]) T(std::forward<Args>(args)...);
			m_denseToSparse[m_size] = sparseIdx;
			++m_size;

			return HandleType(sparseIdx, generation);
		}

		void Erase(HandleType handle)
		{
			if (!IsValid(handle)) return;

			IndexType sparseIdx    = handle.GetIndex();
			IndexType denseIdx     = m_sparse[sparseIdx].NextFree;
			IndexType lastDenseIdx = static_cast<IndexType>(m_size - 1);

			if (denseIdx != lastDenseIdx)
			{
				m_dense[denseIdx] = std::move(m_dense[lastDenseIdx]);

				IndexType lastSparseIdx = m_denseToSparse[lastDenseIdx];
				m_sparse[lastSparseIdx].NextFree = denseIdx;
				m_denseToSparse[denseIdx]        = lastSparseIdx;
			}

			m_dense[lastDenseIdx].~T();
			--m_size;

			m_sparse[sparseIdx].Generation++;
			m_sparse[sparseIdx].NextFree = m_freeListHead;
			m_freeListHead               = sparseIdx;
		}

	private:
		static constexpr IndexType INVALID_IDX = std::numeric_limits<IndexType>::max();

		struct Slot
		{
			IndexType NextFree;
			IndexType Generation;
		};

		// Dense storage uses aligned raw bytes to support non-default-constructible T
		alignas(T) std::array<std::byte, sizeof(T)* Capacity> m_denseStorage;
		T* m_dense = reinterpret_cast<T*>(m_denseStorage.data());

		std::array<Slot, Capacity>      m_sparse;
		std::array<IndexType, Capacity> m_denseToSparse;
		size_t                          m_size = 0;
		IndexType                       m_freeListHead = INVALID_IDX;
	};
}
