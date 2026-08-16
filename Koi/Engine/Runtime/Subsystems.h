#pragma once
#include "Koi/Engine/Core/Types.h"
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace koi
{
	class Window;

	// Handed to every subsystem's Init, once the main window exists
	struct SubsystemInitContext
	{
		Window&          MainWindow;
		bool             VSync = false;
		std::string_view RenderPipeline;
	};

	template<typename... Ts>
	struct TypeList {};

	namespace internal
	{
		template<typename T, typename... Ts>
		inline constexpr bool ContainsType = (std::is_same_v<T, Ts> || ...);

		// Position of T in Ts..., or sizeof...(Ts) when absent
		template<typename T, typename... Ts>
		consteval std::size_t IndexOfType()
		{
			std::size_t index   = sizeof...(Ts);
			std::size_t current = 0;
			((std::is_same_v<T, Ts> && index == sizeof...(Ts) ? (void)(index = current) : (void)0, ++current), ...);
			return index;
		}

		template<typename T>
		consteval auto DeclaredDependencies()
		{
			if constexpr (requires { typename T::Dependencies; })
				return typename T::Dependencies{};
			else
				return TypeList<>{};
		}

		// Its own type so a failure names both subsystem and dependency in the instantiation stack
		template<typename Subsystem, typename Dependency, bool Registered, bool Ordered>
		struct ValidateDependency
		{
			static_assert(Registered, "Subsystem declares a dependency that is not registered in this SubsystemSet");
			static_assert(Ordered,    "Subsystem must be declared after every type in its Dependencies list");
			static constexpr bool Ok = true;
		};
	}

	// Compile time, dependency checked. Declaration order is init order, shutdown in reverse
	template<typename... Ts>
	class SubsystemSet : public NonCopyable, public NonMovable
	{
		static_assert(sizeof...(Ts) > 0, "SubsystemSet needs at least one subsystem");

	public:
		// Constructs every subsystem in declaration order
		void ConstructAll()
		{
			(std::get<std::optional<Ts>>(m_systems).emplace(), ...);
		}

		// Calls Init(args...) | Init() - on each subsystem that has one
		template<typename... Args>
		void InitAll(Args&... args)
		{
			(InitOne<Ts>(args...), ...);
		}

		// Reverse order: calls Shutdown(args...) | Shutdown() - then destroys
		template<typename... Args>
		void ShutdownAll(Args&... args)
		{
			ShutdownReverse(std::index_sequence_for<Ts...>{}, args...);
		}

		template<typename T>
		[[nodiscard]] T& Get() noexcept
		{
			static_assert(internal::ContainsType<T, Ts...>, "Subsystem is not registered in this SubsystemSet");
			return *std::get<std::optional<T>>(m_systems);
		}

		template<typename T>
		[[nodiscard]] const T& Get() const noexcept
		{
			static_assert(internal::ContainsType<T, Ts...>, "Subsystem is not registered in this SubsystemSet");
			return *std::get<std::optional<T>>(m_systems);
		}

	private:
		template<typename T, typename... Deps>
		static consteval bool ValidateOne(TypeList<Deps...>)
		{
			return (internal::ValidateDependency<T, Deps,
				internal::ContainsType<Deps, Ts...>,
				!internal::ContainsType<Deps, Ts...> || (internal::IndexOfType<Deps, Ts...>() < internal::IndexOfType<T, Ts...>())>::Ok && ...);
		}

		static constexpr bool kDependenciesValid = (ValidateOne<Ts>(internal::DeclaredDependencies<Ts>()) && ...);
		static_assert(kDependenciesValid);

		template<typename T, typename... Args>
		void InitOne(Args&... args)
		{
			T& system = Get<T>();
			if constexpr (requires { system.Init(args...); })
				system.Init(args...);
			else if constexpr (requires { system.Init(); })
				system.Init();
		}

		template<std::size_t... Is, typename... Args>
		void ShutdownReverse(std::index_sequence<Is...>, Args&... args)
		{
			(ShutdownOne<std::tuple_element_t<sizeof...(Ts) - 1 - Is, std::tuple<Ts...>>>(args...), ...);
		}

		template<typename T, typename... Args>
		void ShutdownOne(Args&... args)
		{
			std::optional<T>& slot = std::get<std::optional<T>>(m_systems);
			if (!slot)
				return;

			if constexpr (requires { slot->Shutdown(args...); })
				slot->Shutdown(args...);
			else if constexpr (requires { slot->Shutdown(); })
				slot->Shutdown();

			slot.reset();
		}

	private:
		std::tuple<std::optional<Ts>...> m_systems;
	};
}
