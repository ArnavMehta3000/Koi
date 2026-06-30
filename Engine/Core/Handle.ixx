module;
#include <cassert>
#include <compare>
#include <limits>
#include <vector>
#include <array>
#include <span>
export module Koi.Core.Handle;

import Koi.Core.Types;

export namespace koi
{
	template <typename T, size_t IndexBits = 20, size_t GenerationBits = 12>
	class Handle
	{
		static_assert(IndexBits + GenerationBits <= 64, "IndexBits + GenerationBits must be <= 64");

	public:
		using IndexType = std::conditional_t<(IndexBits + GenerationBits <= 32), u32, u64>;

		static constexpr size_t    BIT_WIDTH       = sizeof(IndexType) * CHAR_BIT;
		static constexpr IndexType INDEX_MASK      = IndexBits      == 0 ? IndexType(0) : (~IndexType(0) >> (BIT_WIDTH - IndexBits));
		static constexpr IndexType GENERATION_MASK = GenerationBits == 0 ? IndexType(0) : (~IndexType(0) >> (BIT_WIDTH - GenerationBits));
		static constexpr IndexType INVALID_ID      = std::numeric_limits<IndexType>::max();

		constexpr Handle() noexcept = default;

		constexpr Handle(IndexType index, IndexType generation) noexcept
			: m_id((index & INDEX_MASK) | ((generation & GENERATION_MASK) << IndexBits))
		{
			assert(index <= INDEX_MASK && generation <= GENERATION_MASK && "index or generation truncated");
		}

		static constexpr Handle FromID(IndexType id) noexcept { return Handle(id & INDEX_MASK, (id >> IndexBits) & GENERATION_MASK); }

		[[nodiscard]] constexpr inline IndexType GetIndex()      const noexcept { return m_id & INDEX_MASK; }
		[[nodiscard]] constexpr inline IndexType GetGeneration() const noexcept { return (m_id >> IndexBits) & GENERATION_MASK; }
		[[nodiscard]] constexpr IndexType        GetId()         const noexcept { return m_id; }
		[[nodiscard]] constexpr bool             IsValid()       const noexcept { return GetIndex() != INDEX_MASK; }

		constexpr explicit operator bool() const noexcept { return IsValid(); }
		auto operator<=>(const Handle&) const = default;

	private:
		IndexType m_id = INVALID_ID;
	};

	template <typename T, typename Tag = T, size_t IndexBits = 20, size_t GenBits = 12>
	class HandlePool
	{
	public:
		using HandleType = Handle<Tag, IndexBits, GenBits>;
		using IndexType  = typename HandleType::IndexType;

		HandlePool() = default;

		[[nodiscard]] u64 GetSize()     const noexcept { return m_dense.size(); }
		[[nodiscard]] u64 GetCapacity() const noexcept { return m_dense.capacity(); }
		[[nodiscard]] inline std::span<T>       GetData() noexcept       { return m_dense; }
		[[nodiscard]] inline std::span<const T> GetData() const noexcept { return m_dense; }

		void Reserve(size_t n)
		{
			m_sparse.reserve(n);
			m_denseToSparse.reserve(n);
			m_dense.reserve(n);
		}

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
			m_freeListHead = INVALID_IDX;

			const auto count = static_cast<IndexType>(m_sparse.size());
			for (IndexType i = 0; i < count; ++i)
			{
				m_sparse[i].Generation++;
				m_sparse[i].Alive    = false;
				m_sparse[i].NextFree = m_freeListHead;
				m_freeListHead       = i;
			}
		}

		void Reset()
		{
			m_dense.clear();
			m_denseToSparse.clear();
			m_sparse.clear();
			m_freeListHead = INVALID_IDX;
		}

		[[nodiscard]] bool IsValid(HandleType handle) const noexcept
		{
			if (!handle) return false;
			IndexType index = handle.GetIndex();
			if (index >= static_cast<IndexType>(m_sparse.size())) return false;
			const Slot& slot = m_sparse[index];
			return slot.Alive && slot.Generation == handle.GetGeneration();
		}

		template <typename... Args>
		[[nodiscard]] HandleType Emplace(Args&&... args)
		{
			IndexType sparseIdx;
			IndexType generation;

			if (m_freeListHead != INVALID_IDX)
			{
				sparseIdx      = m_freeListHead;
				m_freeListHead = m_sparse[sparseIdx].NextFree;
				generation     = m_sparse[sparseIdx].Generation;
			}
			else
			{
				sparseIdx = static_cast<IndexType>(m_sparse.size());
				assert(sparseIdx < HandleType::INDEX_MASK && "Too many handles in pool!");
				generation = 0;
				m_sparse.push_back({ 0, generation });
			}

			IndexType denseIdx = static_cast<IndexType>(m_dense.size());
			m_sparse[sparseIdx].NextFree = denseIdx;
			m_sparse[sparseIdx].Alive    = true;

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
			m_sparse[sparseIdx].Alive    = false;
			m_freeListHead = sparseIdx;
		}

		template <typename Fn>
		void ForEach(Fn&& fn)
		{
			for (IndexType denseIdx = 0; denseIdx < m_dense.size(); ++denseIdx)
			{
				IndexType sparseIdx = m_denseToSparse[denseIdx];
				HandleType h(sparseIdx, m_sparse[sparseIdx].Generation);
				fn(h, m_dense[denseIdx]);
			}
		}

	private:
		static constexpr IndexType INVALID_IDX = std::numeric_limits<IndexType>::max();

		struct Slot
		{
			IndexType NextFree;
			IndexType Generation;
			bool Alive = false;
		};

		std::vector<Slot>      m_sparse;
		std::vector<IndexType> m_denseToSparse;
		std::vector<T>         m_dense;
		IndexType              m_freeListHead = INVALID_IDX;
	};
}

export namespace std
{
	template <typename T, size_t I, size_t G>
	struct hash<koi::Handle<T, I, G>>
	{
		size_t operator()(const koi::Handle<T, I, G>& h) const noexcept
		{
			return std::hash<typename koi::Handle<T, I, G>::IndexType>{}(h.GetId());
		}
	};
}
