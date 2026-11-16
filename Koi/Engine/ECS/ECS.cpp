#include "Koi/Engine/ECS/ECS.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
#pragma region ECS
	ECS::ECS(World& owner)
		: m_owner(&owner)
	{
		m_root = Spawn("Root").GetHandle();
		RegisterPhase<FixedUpdate>();
		RegisterPhase<PreUpdate>();
		RegisterPhase<Update>();
		RegisterPhase<PostUpdate>();
		RegisterPhase<PreRender>();
	}

	ECS::~ECS()
	{
		KOI_SCOPE("ECS Teardown");
		KOI_LOG(ECS, Debug, "Tearing down ECS ({} entities, {} free slots)", m_nodes.size() - m_free.size(), m_free.size());

		// Destroy signals still need the pools, the hooks and whatever the systems own
		Clear();
		DestroyNow(m_root);
		FlushCommands();
	}

	void ECS::Clear()
	{
		Sync();
		for (u32 i = 0; i < m_nodes.size(); ++i)
		{
			const Node& node = m_nodes[i];
			if (node.Alive && !node.Parent.IsValid() && i != m_root.GetIndex())
			{
				DestroyNow(EntityHandle(i, node.Generation));
			}
		}

		for (const std::unique_ptr<internal::PoolBase>& pool : m_pools)
		{
			if (pool) pool->Erase(*this, m_root);
		}
		FlushCommands();
	}

	Entity ECS::Spawn(std::string_view name)
	{
		u32 index;
		if (!m_free.empty())
		{
			index = m_free.back();
			m_free.pop_back();
		}
		else
		{
			index = static_cast<u32>(m_nodes.size());
			KOI_ASSERT(index < EntityHandle::INDEX_MASK, "Out of entity indices");
			m_nodes.emplace_back();
		}

		Node& node = m_nodes[index];
		node.Alive = true;
		node.Name  = name.empty() ? std::string("Entity") : std::string(name);

		const Entity e(this, EntityHandle(index, node.Generation));
		if (m_playing) Emit<BeginPlay>(e);
		return e;
	}

	bool ECS::IsAlive(EntityHandle handle) const
	{
		const u32 index = handle.GetIndex();
		return handle.IsValid() && index < m_nodes.size() && m_nodes[index].Alive && m_nodes[index].Generation == handle.GetGeneration();
	}

	void ECS::EndIteration()
	{
		KOI_ASSERT(m_iterating > 0, "Unbalanced iteration");
		if (--m_iterating == 0) FlushCommands();
	}

	void ECS::FlushCommands()
	{
		if (m_commands.empty()) return;

		KOI_SCOPE("ECS Flush");
		while (!m_commands.empty())
		{
			FrameStats::Count("ECS Commands", static_cast<i64>(m_commands.size()));
			std::vector<std::move_only_function<void()>> batch = std::exchange(m_commands, {});
			for (auto& command : batch) command();
		}
	}

	void ECS::Sync()
	{
		KOI_SCOPE("ECS Sync");
		for (u32 pass = 0; ; ++pass)
		{
			if (pass == 64)
			{
				KOI_LOG(ECS, Error, "Events keep emitting events; giving up on this sync after {} passes", pass);
				KOI_ASSERT(false, "Sync never settles");
				break;
			}
			FlushCommands();

			bool drained = false;
			for (const auto& queue : m_events)
			{
				if (queue) drained |= queue->Drain();
			}
			if (!drained && m_commands.empty()) break;
		}
	}

	u32 ECS::InsertPhase(u32 id, std::string_view name, bool fixedStep, u32 anchor, bool after)
	{
		if (id >= m_phases.size()) m_phases.resize(id + 1);
		m_phases[id] = std::make_unique<PhaseData>(PhaseData{ .Name = name, .FixedStep = fixedStep, .Anchor = anchor, .AnchorAfter = after });

		auto pos = m_phaseOrder.end();
		if (anchor != kNoPhase)
		{
			pos = std::ranges::find(m_phaseOrder, anchor);
			if (after)
			{
				// Land after the anchor and anything already placed after it
				const auto placedUnder = [&](u32 phase)
				{
					for (u32 a = m_phases[phase]->Anchor; a != kNoPhase; a = m_phases[a]->Anchor)
					{
						if (a == anchor) return true;
					}
					return false;
				};
				for (++pos; pos != m_phaseOrder.end() && placedUnder(*pos); ++pos) { /* Empty on purpose */ }
			}
		}
		m_phaseOrder.insert(pos, id);

		KOI_LOG(ECS, Debug, "Phase '{}' registered{}{}{}", name, anchor == kNoPhase ? "" : (after ? " after '" : " before '"),
			anchor == kNoPhase ? "" : m_phases[anchor]->Name, anchor == kNoPhase ? "" : "'");
		return id;
	}

	void ECS::AddSystem(u32 phase, SystemDesc desc)
	{
		PhaseData& data = *m_phases[phase];
		data.Systems.push_back(std::move(desc));
		data.Sorted = false;
	}

	void ECS::SortSystems(PhaseData& phase)
	{
		std::vector<SystemDesc>& systems = phase.Systems;
		const size_t count = systems.size();
		const auto indexOf = [&](u32 id) -> size_t
		{
			const auto it = std::ranges::find(systems, id, &SystemDesc::Id);
			return it == systems.end() ? count : static_cast<size_t>(it - systems.begin());
		};

		std::vector<std::vector<size_t>> next(count);
		std::vector<u32> waiting(count, 0);
		for (size_t i = 0; i < count; ++i)
		{
			for (const u32 id : systems[i].After)
			{
				if (const size_t j = indexOf(id); j != count) { next[j].push_back(i); ++waiting[i]; }
			}
			for (const u32 id : systems[i].Before)
			{
				if (const size_t j = indexOf(id); j != count) { next[i].push_back(j); ++waiting[j]; }
			}
		}

		// Kahn's algorithm: always taking the earliest registered ready system
		std::vector<size_t> order;
		std::vector<bool> placed(count, false);
		while (order.size() < count)
		{
			size_t pick = count;
			for (size_t i = 0; i < count && pick == count; ++i)
			{
				if (!placed[i] && waiting[i] == 0) pick = i;
			}
			if (pick == count)
			{
				KOI_LOG(ECS, Error, "Systems in phase '{}' have a RunsAfter/RunsBefore cycle; the rest run in registration order", phase.Name);
				for (size_t i = 0; i < count; ++i)
				{
					if (!placed[i]) order.push_back(i);
				}
				break;
			}
			placed[pick] = true;
			order.push_back(pick);

			for (const size_t j : next[pick])
			{
				--waiting[j];
			}
		}

		std::vector<SystemDesc> sorted;
		sorted.reserve(count);
		std::string names;
		for (const size_t i : order)
		{
			names += names.empty() ? "" : ", ";
			names += systems[i].Name;
			sorted.push_back(std::move(systems[i]));
		}
		systems = std::move(sorted);
		phase.Sorted = true;
		KOI_LOG(ECS, Debug, "Phase '{}' system order: {}", phase.Name, names.empty() ? "(none)" : names);
	}

	void ECS::RunPhase(PhaseData& phase, f32 dt)
	{
		if (!phase.Sorted) SortSystems(phase);

		KOI_SCOPE_TRANSIENT(phase.Name);
		for (SystemDesc& system : phase.Systems)
		{
			system.Run(dt);
		}
		Sync();
	}

	void ECS::RunPhases(bool fixedStep, f32 dt)
	{
		for (const u32 id : m_phaseOrder)
		{
			PhaseData& phase = *m_phases[id];
			if (phase.FixedStep == fixedStep) RunPhase(phase, dt);
		}
	}

	void ECS::RemoveNow(u32 componentId, EntityHandle handle)
	{
		if (internal::PoolBase* pool = FindPool(componentId))
		{
			pool->Erase(*this, handle);
		}
	}

	void ECS::DestroyNow(EntityHandle handle)
	{
		if (!IsAlive(handle)) return;

		// Children first, so a child's hooks still see its parent
		for (const Entity child : Children(handle))
		{
			DestroyNow(child.GetHandle());
		}

		for (const auto& pool : m_pools)
		{
			if (pool) pool->Erase(*this, handle);
		}

		Unlink(handle);
		Node& node = m_nodes[handle.GetIndex()];
		node.Alive      = false;
		node.Generation = (node.Generation + 1) & EntityHandle::GENERATION_MASK;
		node.Name.clear();
		node.FirstChild = node.LastChild = {};
		m_free.push_back(handle.GetIndex());
	}

	void ECS::Unlink(EntityHandle child)
	{
		Node& node = m_nodes[child.GetIndex()];
		if (!node.Parent.IsValid()) return;

		Node& parent = m_nodes[node.Parent.GetIndex()];
		if (node.Prev.IsValid()) m_nodes[node.Prev.GetIndex()].Next = node.Next;
		else                     parent.FirstChild = node.Next;
		if (node.Next.IsValid()) m_nodes[node.Next.GetIndex()].Prev = node.Prev;
		else                     parent.LastChild = node.Prev;

		node.Parent = node.Prev = node.Next = {};
	}

	void ECS::SetParent(EntityHandle child, EntityHandle parent)
	{
		KOI_ASSERT(child != m_root, "The root can't be reparented");
		if (IsAlive(parent))
		{
			KOI_ASSERT(parent != child && !Entity(this, parent).IsDescendantOf(Entity(this, child)), "Reparenting under a descendant");
		}

		Unlink(child);
		if (!IsAlive(parent)) return;

		Node& node     = m_nodes[child.GetIndex()];
		Node& newOwner = m_nodes[parent.GetIndex()];
		node.Parent = parent;
		node.Prev   = newOwner.LastChild;
		if (newOwner.LastChild.IsValid()) m_nodes[newOwner.LastChild.GetIndex()].Next = child;
		else                              newOwner.FirstChild = child;
		newOwner.LastChild = child;
	}

	std::vector<Entity> ECS::Children(EntityHandle handle)
	{
		std::vector<Entity> children;
		for (EntityHandle c = m_nodes[handle.GetIndex()].FirstChild; c.IsValid(); c = m_nodes[c.GetIndex()].Next)
		{
			children.push_back(Entity(this, c));
		}
		return children;
	}
#pragma endregion ECS

#pragma region Entity
	void Entity::Destroy() const
	{
		if (!IsAlive()) return;
		KOI_ASSERT(m_handle != m_ecs->GetRoot().GetHandle(), "The root can't be destroyed");

		if (m_ecs->IsDeferring())
		{
			m_ecs->Defer([e = *this] { e.m_ecs->DestroyNow(e.m_handle); });
		}
		else
		{
			m_ecs->DestroyNow(m_handle);
		}
	}

	bool Entity::IsAlive() const
	{
		return m_ecs && m_ecs->IsAlive(m_handle);
	}

	ECS& Entity::GetECS() const
	{
		KOI_ASSERT(m_ecs, "Null entity");
		return *m_ecs;
	}

	std::string_view Entity::GetName() const
	{
		return IsAlive() ? m_ecs->GetName(m_handle) : std::string_view();
	}

	void Entity::SetName(std::string_view name) const
	{
		if (IsAlive()) m_ecs->SetName(m_handle, name);
	}

	Entity Entity::GetParent() const
	{
		if (!IsAlive()) return {};
		const EntityHandle parent = m_ecs->GetParent(m_handle);
		return parent.IsValid() ? Entity(m_ecs, parent) : Entity{};
	}

	void Entity::SetParent(Entity parent) const
	{
		if (IsAlive()) m_ecs->SetParent(m_handle, parent.m_handle);
	}

	std::vector<Entity> Entity::Children() const
	{
		return IsAlive() ? m_ecs->Children(m_handle) : std::vector<Entity>{};
	}

	bool Entity::HasChildren() const
	{
		return IsAlive() && m_ecs->HasChildren(m_handle);
	}

	bool Entity::IsDescendantOf(Entity ancestor) const
	{
		for (Entity p = GetParent(); p; p = p.GetParent())
		{
			if (p == ancestor) return true;
		}
		return false;
	}
#pragma endregion Entity
}
