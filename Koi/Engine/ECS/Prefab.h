#pragma once
#include "Koi/Engine/Assets/PrefabAsset.h"
#include "Koi/Engine/ECS/ComponentClone.h"
#include "Koi/Engine/ECS/ComponentSerializers.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/ECS/PrefabRegistry.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include <array>
#include <concepts>
#include <functional>
#include <span>
#include <string>
#include <tuple>
#include <utility>

namespace koi
{
	namespace internal
	{
		[[nodiscard]] PrefabHandle           AcquirePrefab(std::string_view idOrPath);
		[[nodiscard]] PrefabHandle           AddRefPrefab(PrefabHandle handle);
		void                                 ReleasePrefab(PrefabHandle handle);
		[[nodiscard]] const IPrefabTemplate* FindPrefab(PrefabHandle handle);
		[[nodiscard]] std::string            PrefabIdOf(PrefabHandle handle);
		[[nodiscard]] std::string_view       PrefabPathOf(PrefabHandle handle);
		void WarnUndeclaredComponents(const toml::table& file, std::string_view path, std::span<const std::string_view> declared);
	}

	// Copies of a prefab's components | Allows OnSpawn to modify them before they are applied to entity
	template<typename... Cs>
	struct PrefabStaged
	{
		std::tuple<Transform, Cs...> Components;

		template<typename Comp>
		[[nodiscard]] Comp& Get() { return std::get<Comp>(Components); }
	};

	// C++ declares the components (Transform is implied), a .prefab.toml with `class = Derived::kKey` holds their values.
	// Derived may add `static void OnSpawn(Staged&, Args...)`, which World::Spawn's extra arguments go to
	template<typename Derived, typename... Cs>
	class Prefab : public IPrefabTemplate
	{
		static_assert((!std::same_as<Cs, Transform> && ...), "Transform is implied, don't list it");
		static_assert((CustomDeserializable<Cs> && ...), "Every prefab component needs a Serializer");

	public:
		using Staged = PrefabStaged<Cs...>;

		~Prefab() override
		{
			std::apply([](auto&... c) { (ComponentClone<std::remove_cvref_t<decltype(c)>>::Drop(c), ...); }, m_defaults);
		}

		[[nodiscard]] std::string_view Class() const override { return Derived::kKey; }

		void Load(const toml::table& file, std::string_view path) override
		{
			std::apply([&](auto&... c) { (LoadComponent(file, path, c), ...); }, m_defaults);
			internal::WarnUndeclaredComponents(file, path, kComponentKeys);
		}

		template<typename C>
		[[nodiscard]] const C& GetDefault() const { return std::get<C>(m_defaults); }

		template<typename... Args>
		Entity Instantiate(World& world, Args&&... args) const
		{
			Staged staged{ std::apply([](const auto&... c) { return std::tuple{ ComponentClone<std::remove_cvref_t<decltype(c)>>::Copy(c)... }; }, m_defaults) };

			// If we have a OnSpawn, fire that off here
			if constexpr (requires { &Derived::OnSpawn; })
			{
				static_assert(std::invocable<decltype(&Derived::OnSpawn), Staged&, Args...>, "Spawn's arguments don't match OnSpawn");
				Derived::OnSpawn(staged, std::forward<Args>(args)...);
			}
			else
			{
				static_assert(sizeof...(Args) == 0, "Spawn arguments need an OnSpawn to receive them");
			}

			const Entity e = world.Spawn(Derived::kKey);
			std::apply([&](auto&... c) { (e.Set(std::move(c)), ...); }, staged.Components);
			return e;
		}

	private:
		template<typename C>
		static void LoadComponent(const toml::table& file, std::string_view path, C& out)
		{
			const toml::node* node = file.get(C::kKey);
			if (!node) return;

			if (std::optional<C> value = Serializer<C>::Deserialize(*node))
			{
				ComponentClone<C>::Drop(out);
				out = std::move(*value);
			}
			else
			{
				KOI_LOG(Assets, Warn, "Prefab {} couldn't read its '{}' table, so it keeps the defaults", path, C::kKey);
			}
		}

	private:
		static constexpr std::array<std::string_view, sizeof...(Cs) + 1> kComponentKeys{ Transform::kKey, Cs::kKey... };

		std::tuple<Transform, Cs...> m_defaults;
	};

	// One .prefab.toml of class P. Holds a cache reference of its own, so it can sit in any component
	template<typename P>
	class PrefabRef
	{
	public:
		PrefabRef() = default;
		PrefabRef(const PrefabRef& other) : m_handle(internal::AddRefPrefab(other.m_handle)) {}
		PrefabRef(PrefabRef&& other) noexcept : m_handle(std::exchange(other.m_handle, {})) {}
		PrefabRef& operator=(PrefabRef other) noexcept { std::swap(m_handle, other.m_handle); return *this; }
		~PrefabRef() { internal::ReleasePrefab(m_handle); }

		// An asset id, or a virtual path in hand-written files
		[[nodiscard]] static PrefabRef Load(std::string_view idOrPath)
		{
			PrefabRef ref;
			ref.m_handle = internal::AcquirePrefab(idOrPath);
			if (ref.m_handle && !ref.Get())
			{
				KOI_LOG(Assets, Warn, "{} isn't a '{}' prefab", idOrPath, P::kKey);
				return {};
			}
			return ref;
		}

		[[nodiscard]] const P* Get() const
		{
			const IPrefabTemplate* prefab = internal::FindPrefab(m_handle);
			return prefab && prefab->Class() == P::kKey ? static_cast<const P*>(prefab) : nullptr;
		}

		[[nodiscard]] PrefabHandle     GetHandle() const { return m_handle; }
		[[nodiscard]] std::string_view GetPath()   const { return internal::PrefabPathOf(m_handle); }
		explicit operator bool() const { return Get() != nullptr; }
		[[nodiscard]] bool operator==(const PrefabRef& other) const { return m_handle == other.m_handle; }

	private:
		PrefabHandle m_handle;
	};

	template<typename P, typename... Args>
	Entity World::Spawn(const PrefabRef<P>& prefab, Args&&... args)
	{
		const P* source = prefab.Get();
		if (!source)
		{
			KOI_LOG(Scene, Warn, "Tried to spawn an empty '{}' prefab reference", P::kKey);
			return {};
		}
		return source->Instantiate(*this, std::forward<Args>(args)...);
	}
}

namespace rfl
{
	template<typename P>
	struct Reflector<koi::PrefabRef<P>>
	{
		using ReflType = std::string;

		static std::string from(const koi::PrefabRef<P>& ref) { return koi::internal::PrefabIdOf(ref.GetHandle()); }
		static koi::PrefabRef<P> to(const std::string& id) { return id.empty() ? koi::PrefabRef<P>() : koi::PrefabRef<P>::Load(id); }
	};
}
