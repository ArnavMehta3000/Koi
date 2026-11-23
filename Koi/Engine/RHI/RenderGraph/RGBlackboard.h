#pragma once
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Core/TypeIndex.h"
#include "Koi/Engine/Core/Types.h"
#include <concepts>
#include <memory>
#include <vector>

namespace koi
{
	template <typename T>
	concept BlackboardEntry = std::is_class_v<T> && std::movable<T> && std::default_initializable<T>;

	// One entry per type, published by the pass that produces it; cleared with the graph every frame
	class RGBlackboard : NonCopyable
	{
		struct SlotBase
		{
			virtual ~SlotBase() = default;
			bool Present = false;
		};

		template <BlackboardEntry T>
		struct Slot : SlotBase
		{
			T Value{};
		};

	public:
		template <BlackboardEntry T>
		T& Add(T value = {})
		{
			Slot<T>& slot = SlotOf<T>();
			KOI_ASSERT(!slot.Present, "Blackboard entry added twice in one frame");
			slot.Value   = std::move(value);
			slot.Present = true;
			return slot.Value;
		}

		template <BlackboardEntry T>
		[[nodiscard]] T& Get()
		{
			T* value = TryGet<T>();
			KOI_ASSERT(value, "Blackboard entry read before any pass added it");
			return *value;
		}

		template <BlackboardEntry T>
		[[nodiscard]] T* TryGet()
		{
			const u32 index = IndexOf<T>();
			if (index >= m_slots.size() || !m_slots[index] || !m_slots[index]->Present) return nullptr;
			return &static_cast<Slot<T>*>(m_slots[index].get())->Value;
		}

		template <BlackboardEntry T>
		[[nodiscard]] bool Has() { return TryGet<T>() != nullptr; }

		// Slots stay allocated, so steady-state frames don't allocate
		void Clear()
		{
			for (auto& slot : m_slots)
			{
				if (slot) slot->Present = false;
			}
		}

	private:
		template <BlackboardEntry T>
		static u32 IndexOf()
		{
			return internal::TypeIndex<RGBlackboard, T>();
		}

		template <BlackboardEntry T>
		Slot<T>& SlotOf()
		{
			const u32 index = IndexOf<T>();
			if (index >= m_slots.size())
			{
				m_slots.resize(index + 1);
			}
			if (!m_slots[index])
			{
				m_slots[index] = std::make_unique<Slot<T>>();
			}
			return *static_cast<Slot<T>*>(m_slots[index].get());
		}

	private:
		std::vector<std::unique_ptr<SlotBase>> m_slots;
	};
}
