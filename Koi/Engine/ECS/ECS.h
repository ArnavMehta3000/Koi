#pragma once
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Core/TypeIndex.h"
#include "Koi/Engine/Core/Types.h"
#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace koi
{
	class ECS;
	class World;

	struct EntityTag;
	using EntityHandle = Handle<EntityTag>;
	using EntityID     = u64;

	template<typename T>
	concept Component = std::is_object_v<T> && std::move_constructible<std::remove_const_t<T>>;

	// Placement of a custom phase
	template<typename P> struct After  { using Anchor = P; static constexpr bool kAfter = true; };
	template<typename P> struct Before { using Anchor = P; static constexpr bool kAfter = false; };

	template<typename Placement = void, bool FixedStep = false>
	struct Phase
	{
		using PlacementType = Placement;
		static constexpr bool kFixedStep = FixedStep;
	};

	struct FixedUpdate : Phase<void, true> {};
	struct PreUpdate   : Phase<> {};
	struct Update      : Phase<> {};
	struct PostUpdate  : Phase<> {};
	struct PreRender   : Phase<> {};

	// For a system's RunsAfter / RunsBefore
	template<typename... Systems> struct SystemList {};

	// Sent to every entity when play starts or stops, and to entities spawned during play
	struct BeginPlay {};
	struct EndPlay {};

	// Sent to persistent entities
	struct SceneLoaded {};

	namespace internal
	{
		struct ComponentDomain {};
		struct EventDomain {};
		struct PhaseDomain {};
		struct SystemDomain {};

		template<typename T> u32 ComponentIdOf() { return TypeIndex<ComponentDomain, T>(); }
		template<typename T> u32 EventIdOf()     { return TypeIndex<EventDomain, T>();     }
		template<typename T> u32 PhaseIdOf()     { return TypeIndex<PhaseDomain, T>();     }
		template<typename T> u32 SystemIdOf()    { return TypeIndex<SystemDomain, T>();    }

		using refl::TypeName;
	}

	// A handle to an entity in an ECS. Null when default-constructed; a const component type means read-only access
	class Entity
	{
	public:
		Entity() = default;

		template<Component T> [[nodiscard]] T& Get() const;
		template<Component T> [[nodiscard]] T* TryGet() const;
		template<Component T> [[nodiscard]] bool Has() const;

		template<Component T, typename... Args> const Entity& Add(Args&&... args) const;
		template<Component T> const Entity& Set(T value) const;
		template<Component T> const Entity& Remove() const;

		void Destroy() const;
		[[nodiscard]] bool IsAlive() const;
		[[nodiscard]] EntityID GetId() const { return m_handle.GetId(); }
		[[nodiscard]] EntityHandle GetHandle() const { return m_handle; }
		[[nodiscard]] ECS& GetECS() const;

		[[nodiscard]] std::string_view GetName() const;
		void SetName(std::string_view name) const;

		[[nodiscard]] Entity GetParent() const;
		void SetParent(Entity parent) const;  // a null parent detaches
		[[nodiscard]] std::vector<Entity> Children() const;
		[[nodiscard]] bool HasChildren() const;
		[[nodiscard]] bool IsDescendantOf(Entity ancestor) const;

		explicit operator bool() const { return IsAlive(); }
		bool operator==(const Entity&) const = default;

	private:
		friend class ECS;
		Entity(ECS* ecs, EntityHandle handle) : m_ecs(ecs), m_handle(handle) {}

	private:
		ECS*         m_ecs = nullptr;
		EntityHandle m_handle;
	};

	namespace internal
	{
		// Sparse set keyed by entity index: sparse[index] -> dense slot
		class PoolBase
		{
		public:
			virtual ~PoolBase() = default;

			[[nodiscard]] bool Has(u32 index) const { return index < m_sparse.size() && m_sparse[index] != kNone; }
			[[nodiscard]] size_t Size() const { return m_dense.size(); }
			[[nodiscard]] std::span<const EntityHandle> Entities() const { return m_dense; }

			virtual void Erase(ECS& ecs, EntityHandle handle) = 0;  // fires the remove signal first

		protected:
			static constexpr u32 kNone = ~0u;

			void Insert(EntityHandle handle)
			{
				const u32 index = handle.GetIndex();
				if (index >= m_sparse.size()) m_sparse.resize(index + 1, kNone);
				m_sparse[index] = static_cast<u32>(m_dense.size());
				m_dense.push_back(handle);
			}

		protected:
			std::vector<u32>          m_sparse;
			std::vector<EntityHandle> m_dense;
		};

		template<typename T>
		class Pool final : public PoolBase
		{
		public:
			using Signal = std::move_only_function<void(Entity, T&)>;

			[[nodiscard]] T* TryGet(u32 index) { return Has(index) ? &m_data[m_sparse[index]] : nullptr; }
			[[nodiscard]] T& At(u32 index) { return m_data[m_sparse[index]]; }

			template<typename... Args>
			T& Emplace(EntityHandle handle, Args&&... args)
			{
				Insert(handle);
				if constexpr (sizeof...(Args) == 0) return m_data.emplace_back();
				else                                return m_data.emplace_back(T{ std::forward<Args>(args)... });
			}

			void Erase(ECS& ecs, EntityHandle handle) override;

			void OnAdd(Signal fn)    { m_onAdd.push_back(std::move(fn)); }
			void OnRemove(Signal fn) { m_onRemove.push_back(std::move(fn)); }

			void NotifyAdd(Entity e)
			{
				for (Signal& fn : m_onAdd) fn(e, At(e.GetHandle().GetIndex()));
			}

		private:
			std::vector<T>      m_data;
			std::vector<Signal> m_onAdd;
			std::vector<Signal> m_onRemove;
		};

		class EventQueueBase
		{
		public:
			virtual ~EventQueueBase() = default;
			virtual bool Drain() = 0;  // false when nothing was pending
		};

		template<typename Event>
		class EventQueue final : public EventQueueBase
		{
		public:
			using Observer = std::move_only_function<void(Entity, const Event&)>;

			void Push(Entity e, const Event& event) { if (!m_observers.empty()) m_pending.emplace_back(e, event); }
			void Subscribe(Observer fn) { m_observers.push_back(std::move(fn)); }

			bool Drain() override
			{
				if (m_pending.empty()) return false;
				std::vector<std::pair<Entity, Event>> batch = std::exchange(m_pending, {});
				for (const auto& [e, event] : batch)
				{
					if (!e.IsAlive()) continue;
					for (Observer& observer : m_observers)
					{
						observer(e, event);
					}
				}
				return true;
			}

		private:
			std::vector<std::pair<Entity, Event>> m_pending;
			std::vector<Observer>                 m_observers;
		};
	}

	template<Component... Cs>
	class Query
	{
		friend class ECS;

	public:
		template<Component C> Query& With()    { m_with.push_back(internal::ComponentIdOf<std::remove_const_t<C>>());    return *this; }
		template<Component C> Query& Without() { m_without.push_back(internal::ComponentIdOf<std::remove_const_t<C>>()); return *this; }

		// fn(Entity, Cs&...) or fn(Cs&...)
		template<typename Fn> void Each(Fn&& fn);

	private:
		explicit Query(ECS* ecs) : m_ecs(ecs) {}

		template<typename Fn, size_t... I>
		void EachImpl(Fn& fn, std::index_sequence<I...>);

	private:
		ECS*             m_ecs;
		std::vector<u32> m_with;
		std::vector<u32> m_without;
	};

	class ECS : public NonCopyable
	{
	public:
		explicit ECS(World& owner);
		~ECS();
		ECS(ECS&&) = delete;
		ECS& operator=(ECS&&) = delete;

		[[nodiscard]] World& GetWorld() const { return *m_owner; }

		[[nodiscard]] Entity Spawn(std::string_view name = {});
		[[nodiscard]] Entity FromId(EntityID id) { return Entity(this, EntityHandle::FromID(static_cast<EntityHandle::IndexType>(id))); }
		[[nodiscard]] Entity GetRoot() { return Entity(this, m_root); }

		// const because iteration is not a structural change; write access comes from the component types
		template<Component... Cs> [[nodiscard]] koi::Query<Cs...> Query() const { return koi::Query<Cs...>(const_cast<ECS*>(this)); }
		template<Component... Cs, typename Fn> void Each(Fn&& fn) const { Query<Cs...>().Each(std::forward<Fn>(fn)); }

		// Singletons live on the root entity
		template<Component T, typename... Args> void Add(Args&&... args) { GetRoot().Add<T>(std::forward<Args>(args)...); }
		template<Component T> [[nodiscard]] bool Has() const { return const_cast<ECS*>(this)->GetRoot().Has<T>(); }
		template<Component T> [[nodiscard]] T* TryGet() { return GetRoot().TryGet<T>(); }
		template<Component T> [[nodiscard]] T& Get() { return GetRoot().Get<T>(); }
		template<Component T> void Remove() { GetRoot().Remove<T>(); }

		// fn(Entity, T&), whenever T leaves an entity
		template<Component T, typename Fn> void OnRemove(Fn&& fn) { GetPool<T>().OnRemove(std::forward<Fn>(fn)); }

		// fn(Entity, T&), right after T lands on an entity
		template<Component T, typename Fn> void OnAdd(Fn&& fn) { GetPool<T>().OnAdd(std::forward<Fn>(fn)); }

		// Events are queued and delivered at the next sync point
		template<typename Event> void Emit(Entity e, const Event& event = {}) { GetEventQueue<Event>().Push(e, event); }
		template<typename Event, typename Fn> void Observe(Fn&& fn) { GetEventQueue<Event>().Subscribe(std::forward<Fn>(fn)); }

		using SystemFn = std::move_only_function<void(f32)>;

		struct SystemDesc
		{
			u32              Id = 0;
			std::string_view Name;
			std::vector<u32> After;
			std::vector<u32> Before;
			SystemFn         Run;
		};

		// Registers P (and its anchor) on first use; returns its id
		template<typename P> u32 RegisterPhase();
		void AddSystem(u32 phase, SystemDesc desc);

		// Runs every fixed-step (or every frame) phase in order, each followed by a sync point
		void RunPhases(bool fixedStep, f32 dt);

		// While playing, spawned entities get BeginPlay
		void SetPlaying(bool playing) { m_playing = playing; }
		template<typename Event> void EmitToAll(const Event& event = {}, const std::function<bool(Entity)>& filter = {});

		// Flushes deferred structural changes and drains events until both are empty
		void Sync();

		// Destroys every top-level entity keep() doesn't claim, with its subtree, and strips the root's singletons
		void Clear(const std::function<bool(Entity)>& keep = {});

		// Internal helpers
		[[nodiscard]] bool IsAlive(EntityHandle handle) const;
		[[nodiscard]] bool IsDeferring() const { return m_iterating > 0; }
		void Defer(std::move_only_function<void()> command) { m_commands.push_back(std::move(command)); }
		void BeginIteration() { ++m_iterating; }
		void EndIteration();

		template<Component T> [[nodiscard]] internal::Pool<std::remove_const_t<T>>& GetPool();
		template<Component T> [[nodiscard]] internal::Pool<std::remove_const_t<T>>* FindPool() const;
		[[nodiscard]] internal::PoolBase* FindPool(u32 componentId) const { return componentId < m_pools.size() ? m_pools[componentId].get() : nullptr; }

		void DestroyNow(EntityHandle handle);
		void RemoveNow(u32 componentId, EntityHandle handle);

		[[nodiscard]] std::string_view GetName(EntityHandle handle) const { return m_nodes[handle.GetIndex()].Name; }
		void SetName(EntityHandle handle, std::string_view name) { m_nodes[handle.GetIndex()].Name = name; }

		[[nodiscard]] EntityHandle GetParent(EntityHandle handle) const { return m_nodes[handle.GetIndex()].Parent; }
		void SetParent(EntityHandle child, EntityHandle parent);
		[[nodiscard]] std::vector<Entity> Children(EntityHandle handle);
		[[nodiscard]] bool HasChildren(EntityHandle handle) const { return m_nodes[handle.GetIndex()].FirstChild.IsValid(); }

	private:
		template<typename Event> internal::EventQueue<Event>& GetEventQueue();

		static constexpr u32 kNoPhase = ~0u;

		struct PhaseData
		{
			std::string_view        Name;
			bool                    FixedStep   = false;
			u32                     Anchor      = kNoPhase;
			bool                    AnchorAfter = true;
			bool                    Sorted      = true;
			std::vector<SystemDesc> Systems;
		};

		u32 InsertPhase(u32 id, std::string_view name, bool fixedStep, u32 anchor, bool after);
		void RunPhase(PhaseData& phase, f32 dt);
		void SortSystems(PhaseData& phase);

		void Unlink(EntityHandle child);
		void FlushCommands();

	private:
		// Per-entity record
		struct Node
		{
			u32          Generation = 0;
			bool         Alive      = false;
			std::string  Name;
			EntityHandle Parent, FirstChild, LastChild, Prev, Next;
		};

		std::vector<Node>                                      m_nodes;
		std::vector<u32>                                       m_free;
		std::vector<std::unique_ptr<internal::PoolBase>>       m_pools;
		std::vector<std::unique_ptr<internal::EventQueueBase>> m_events;
		std::vector<std::move_only_function<void()>>           m_commands;
		std::vector<std::unique_ptr<PhaseData>>                m_phases;
		std::vector<u32>                                       m_phaseOrder;
		u32          m_iterating = 0;
		bool         m_playing   = false;
		EntityHandle m_root;
		World*       m_owner;
	};
}

#include "Koi/Engine/ECS/ECS.inl"
