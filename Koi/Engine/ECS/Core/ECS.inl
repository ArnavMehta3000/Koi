#pragma once

namespace koi
{
#pragma region Internal
	template<Component T>
	internal::Pool<std::remove_const_t<T>>& ECS::GetPool()
	{
		using U = std::remove_const_t<T>;
		const u32 id = internal::ComponentIdOf<U>();
		if (id >= m_pools.size()) m_pools.resize(id + 1);
		if (!m_pools[id]) m_pools[id] = std::make_unique<internal::Pool<U>>();
		return static_cast<internal::Pool<U>&>(*m_pools[id]);
	}

	template<Component T>
	internal::Pool<std::remove_const_t<T>>* ECS::FindPool() const
	{
		using U = std::remove_const_t<T>;
		return static_cast<internal::Pool<U>*>(FindPool(internal::ComponentIdOf<U>()));
	}

	template<typename Event>
	internal::EventQueue<Event>& ECS::GetEventQueue()
	{
		const u32 id = internal::EventIdOf<Event>();
		if (id >= m_events.size()) m_events.resize(id + 1);
		if (!m_events[id]) m_events[id] = std::make_unique<internal::EventQueue<Event>>();
		return static_cast<internal::EventQueue<Event>&>(*m_events[id]);
	}

	template<typename P>
	u32 ECS::RegisterPhase()
	{
		const u32 id = internal::PhaseIdOf<P>();
		if (id < m_phases.size() && m_phases[id]) return id;

		using Placement = typename P::PlacementType;
		if constexpr (std::is_void_v<Placement>)
		{
			return InsertPhase(id, internal::TypeName<P>(), P::kFixedStep, kNoPhase, true);
		}
		else
		{
			const u32 anchor = RegisterPhase<typename Placement::Anchor>();
			return InsertPhase(id, internal::TypeName<P>(), P::kFixedStep, anchor, Placement::kAfter);
		}
	}

	template<typename Event>
	void ECS::EmitToAll(const Event& event, const std::function<bool(Entity)>& filter)
	{
		for (u32 i = 0; i < m_nodes.size(); ++i)
		{
			if (!m_nodes[i].Alive || i == m_root.GetIndex()) continue;

			const Entity e(this, EntityHandle(i, m_nodes[i].Generation));
			if (!filter || filter(e))
			{
				Emit(e, event);
			}
		}
	}

	template<typename T>
	void internal::Pool<T>::Erase(ECS& ecs, EntityHandle handle)
	{
		const u32 index = handle.GetIndex();
		if (!Has(index)) return;

		for (Signal& fn : m_onRemove)
		{
			fn(ecs.FromId(handle.GetId()), m_data[m_sparse[index]]);
		}

		const u32 slot = m_sparse[index];
		const u32 last = static_cast<u32>(m_dense.size() - 1);
		if (slot != last)
		{
			m_data[slot]  = std::move(m_data[last]);
			m_dense[slot] = m_dense[last];
			m_sparse[m_dense[slot].GetIndex()] = slot;
		}
		m_data.pop_back();
		m_dense.pop_back();
		m_sparse[index] = kNone;
	}
#pragma endregion Internal

#pragma region Entity
	template<Component T>
	T* Entity::TryGet() const
	{
		if (!IsAlive()) return nullptr;
		auto* pool = m_ecs->FindPool<T>();
		return pool ? pool->TryGet(m_handle.GetIndex()) : nullptr;
	}

	template<Component T>
	T& Entity::Get() const
	{
		T* value = TryGet<T>();
		KOI_ASSERT(value, "Entity has no such component");
		return *value;
	}

	template<Component T>
	bool Entity::Has() const
	{
		if (!IsAlive()) return false;
		const auto* pool = m_ecs->FindPool<T>();
		return pool && pool->Has(m_handle.GetIndex());
	}

	template<Component T, typename... Args>
	const Entity& Entity::Add(Args&&... args) const
	{
		KOI_ASSERT(IsAlive(), "Add on a dead entity");
		if (T* existing = TryGet<T>())
		{
			if constexpr (sizeof...(Args) > 0) *existing = T{ std::forward<Args>(args)... };
			return *this;
		}

		// Swap-remove would break a loop over this pool, so structural changes wait for the sync point
		if (m_ecs->IsDeferring())
		{
			m_ecs->Defer([e = *this, value = T{ std::forward<Args>(args)... }]() mutable
			{
				if (!e.IsAlive()) return;

				if (T* existing = e.TryGet<T>())
				{
					*existing = std::move(value);
				}
				else
				{
					auto& pool = e.m_ecs->GetPool<T>();
					pool.Emplace(e.m_handle, std::move(value));
					pool.NotifyAdd(e);
				}
			});
			return *this;
		}

		auto& pool = m_ecs->GetPool<T>();
		pool.Emplace(m_handle, std::forward<Args>(args)...);
		pool.NotifyAdd(*this);
		return *this;
	}

	template<Component T>
	const Entity& Entity::Set(T value) const
	{
		if (T* existing = TryGet<T>())
		{
			*existing = std::move(value);
			return *this;
		}
		return Add<T>(std::move(value));
	}

	template<Component T>
	const Entity& Entity::Remove() const
	{
		if (!Has<T>()) return *this;

		const u32 id = internal::ComponentIdOf<T>();
		if (m_ecs->IsDeferring())
		{
			m_ecs->Defer([e = *this, id] { if (e.IsAlive()) e.m_ecs->RemoveNow(id, e.m_handle); });
		}
		else
		{
			m_ecs->RemoveNow(id, m_handle);
		}
		return *this;
	}
#pragma endregion Entity

#pragma region Query
	template<Component... Cs>
	template<typename Fn>
	void Query<Cs...>::Each(Fn&& fn)
	{
		EachImpl(fn, std::index_sequence_for<Cs...>{});
	}

	template<Component... Cs>
	template<typename Fn, size_t... I>
	void Query<Cs...>::EachImpl(Fn& fn, std::index_sequence<I...>)
	{
		ECS& ecs = *m_ecs;
		const std::tuple<internal::Pool<std::remove_const_t<Cs>>*...> pools{ ecs.FindPool<Cs>()... };
		if (((std::get<I>(pools) == nullptr) || ...)) return;

		std::vector<internal::PoolBase*> with;
		for (const u32 id : m_with)
		{
			internal::PoolBase* pool = ecs.FindPool(id);
			if (!pool) return;
			with.push_back(pool);
		}

		// Iterate the smallest pool, probe the rest
		internal::PoolBase* driver = std::get<0>(pools);
		((driver = std::get<I>(pools)->Size() < driver->Size() ? std::get<I>(pools) : driver), ...);
		for (internal::PoolBase* pool : with)
		{
			driver = pool->Size() < driver->Size() ? pool : driver;
		}

		struct Guard { ECS& Ecs; ~Guard() { Ecs.EndIteration(); } };
		ecs.BeginIteration();
		const Guard guard{ ecs };

		for (const EntityHandle handle : driver->Entities())
		{
			const u32 index = handle.GetIndex();
			if (!(std::get<I>(pools)->Has(index) && ...)) continue;
			if (std::ranges::any_of(with, [&](internal::PoolBase* p) { return !p->Has(index); })) continue;
			if (std::ranges::any_of(m_without, [&](u32 id) { internal::PoolBase* p = ecs.FindPool(id); return p && p->Has(index); })) continue;

			if constexpr (std::invocable<Fn&, Entity, Cs&...>) fn(ecs.FromId(handle.GetId()), std::get<I>(pools)->At(index)...);
			else                                               fn(std::get<I>(pools)->At(index)...);
		}
	}
#pragma endregion Query
}
