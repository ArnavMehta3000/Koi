#include "Koi/Engine/ECS/ECS.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

namespace koi
{
#pragma region ECS
	ECS::ECS(World& owner)
		: m_owner(&owner)
	{
		m_root = Spawn("Root").GetHandle();
	}

	ECS::~ECS()
	{
		// Destroy signals still need the pools, the hooks and whatever the systems own
		Sync();
		for (u32 i = 0; i < m_nodes.size(); ++i)
		{
			const Node& node = m_nodes[i];
			if (node.Alive && !node.Parent.IsValid() && i != m_root.GetIndex())
			{
				DestroyNow(EntityHandle(i, node.Generation));
			}
		}
		DestroyNow(m_root);
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
		return Entity(this, EntityHandle(index, node.Generation));
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
		while (!m_commands.empty())
		{
			std::vector<std::move_only_function<void()>> batch = std::exchange(m_commands, {});
			for (auto& command : batch) command();
		}
	}

	void ECS::Sync()
	{
		for (u32 pass = 0; ; ++pass)
		{
			KOI_ASSERT(pass < 64, "Events keep emitting events; sync never settles");
			FlushCommands();

			bool drained = false;
			for (const auto& queue : m_events)
			{
				if (queue) drained |= queue->Drain();
			}
			if (!drained && m_commands.empty()) break;
		}
	}

	void ECS::AddSystem(SystemPhase phase, SystemFn fn)
	{
		m_phases[static_cast<size_t>(phase)].push_back(std::move(fn));
	}

	void ECS::RunPhase(SystemPhase phase, f32 dt)
	{
		for (SystemFn& system : m_phases[static_cast<size_t>(phase)])
		{
			system(dt);
		}
		Sync();
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
