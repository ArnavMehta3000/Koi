#pragma once
#include "Koi/Engine/Core/Macros.h"
#include "Koi/Engine/Core/Types.h"
#include <array>
#include <cctype>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

// Use in struct, list every element (including transients)
#define KOI_REFLECT(Type, ...)                                                                                      \
	static constexpr auto KoiReflectFields()                                                                        \
	{                                                                                                               \
		constexpr auto fields = ::std::tuple{ KOI_PP_FOR_EACH(KOI_INTERNAL_REFLECT_FIELD, Type, __VA_ARGS__) };     \
		static_assert(::koi::refl::internal::ListsEveryField<Type>(::std::tuple_size_v<decltype(fields)>),          \
			"KOI_REFLECT(" #Type ", ...) must list every field of " #Type);                                         \
		return fields;                                                                                              \
	}                                                                                                               \
	using KoiReflected = Type

#define KOI_INTERNAL_REFLECT_FIELD(Type, Name) ::koi::refl::Field<Type, decltype(Type::Name)>{ #Name, &Type::Name }

namespace koi
{
	// Non-reflected, non-serialized, non-modifiable in editor
	template <typename T>
	struct Transient
	{
		using Type = T;

		T Value{};

		constexpr Transient() = default;
		constexpr Transient(const T& value) : Value(value) {}
		constexpr Transient(T&& value) : Value(std::move(value)) {}

		[[nodiscard]] constexpr T&       get() noexcept              { return Value; }
		[[nodiscard]] constexpr const T& get() const noexcept        { return Value; }
		[[nodiscard]] constexpr T&       operator*() noexcept        { return Value; }
		[[nodiscard]] constexpr const T& operator*() const noexcept  { return Value; }
		[[nodiscard]] constexpr T*       operator->() noexcept       { return &Value; }
		[[nodiscard]] constexpr const T* operator->() const noexcept { return &Value; }
	};
}

namespace koi::refl
{
	template <typename C, typename F>
	struct Field
	{
		using Class = C;
		using Type  = F;

		std::string_view Name;
		F C::*           Member;
	};

	struct FieldInfo
	{
		std::string_view Name;  // "PopDuration"
		std::string_view Key;   // "pop_duration", the TOML key
		bool             IsTransient = false;
	};

	template <typename E>
	struct EnumEntry
	{
		std::string_view Name;  // null-terminated
		E                Value;
	};

	template <typename T>
	concept Reflected = requires { { T::KoiReflectFields() }; typename T::KoiReflected; }
		&& std::same_as<typename T::KoiReflected, T>;

	template <typename T> inline constexpr bool kIsTransient               = false;
	template <typename T> inline constexpr bool kIsTransient<Transient<T>> = true;

	template <Reflected T>
	inline constexpr auto kFields = T::KoiReflectFields();

	template <Reflected T>
	inline constexpr size_t kFieldCount = std::tuple_size_v<std::remove_cvref_t<decltype(kFields<T>)>>;

	// Unqualified type name from the MSVC signature
	template <typename T>
	constexpr std::string_view TypeName()
	{
		std::string_view name = __FUNCSIG__;
		name = name.substr(name.find("TypeName<") + 9);
		name = name.substr(0, name.rfind(">("));
		if (const size_t space = name.find(' '); space != std::string_view::npos) name.remove_prefix(space + 1);
		if (const size_t colon = name.rfind("::"); colon != std::string_view::npos) name.remove_prefix(colon + 2);
		return name;
	}

	// kKey if presetn or TypeName()
	template <typename T>
	constexpr std::string_view NameOf()
	{
		if constexpr (requires { { T::kKey } -> std::convertible_to<std::string_view>; }) return T::kKey;
		else return TypeName<T>();
	}

	namespace internal
	{
		// Only ever named in unevaluated brace-init, to count an aggregate's fields
		struct AnyField
		{
			template <typename T> operator T() const;
		};

		template <size_t> using AnyFieldAt = AnyField;

		template <typename T, size_t... I>
		consteval bool BraceConstructible(std::index_sequence<I...>)
		{
			return requires { T{ AnyFieldAt<I>{}... }; };
		}

		template <typename T, size_t N>
		consteval size_t AggregateFieldCount()
		{
			if constexpr (N < 64 && BraceConstructible<T>(std::make_index_sequence<N + 1>{}))
				return AggregateFieldCount<T, N + 1>();
			else
				return N;
		}

		template <typename T>
		consteval bool ListsEveryField(size_t listed)
		{
			if constexpr (std::is_aggregate_v<T>) return AggregateFieldCount<T, 0>() == listed;
			else return true;
		}

		// "PopDuration" -> "pop_duration"
		inline std::string SnakeCase(std::string_view name)
		{
			std::string out;
			out.reserve(name.size() + 4);
			for (size_t i = 0; i < name.size(); ++i)
			{
				const unsigned char c = static_cast<unsigned char>(name[i]);
				if (i > 0 && std::isupper(c)) out += '_';
				out += static_cast<char>(std::tolower(c));
			}
			return out;
		}

		template <Reflected T>
		const std::array<FieldInfo, kFieldCount<T>>& FieldInfos()
		{
			using Fields = std::remove_cvref_t<decltype(kFields<T>)>;

			static const std::array<std::string, kFieldCount<T>> keys = []<size_t... I>(std::index_sequence<I...>)
			{
				return std::array<std::string, kFieldCount<T>>{ SnakeCase(std::get<I>(kFields<T>).Name)... };
			}(std::make_index_sequence<kFieldCount<T>>{});

			static const std::array<FieldInfo, kFieldCount<T>> infos = []<size_t... I>(std::index_sequence<I...>)
			{
				return std::array<FieldInfo, kFieldCount<T>>
				{
					FieldInfo
					{
						.Name        = std::get<I>(kFields<T>).Name,
						.Key         = keys[I],
						.IsTransient = kIsTransient<typename std::tuple_element_t<I, Fields>::Type>
					}...
				};
			}(std::make_index_sequence<kFieldCount<T>>{});

			return infos;
		}

		template <auto V>
		constexpr std::string_view EnumProbe()
		{
			std::string_view name = __FUNCSIG__;
			name = name.substr(name.find("EnumProbe<") + 10);
			name = name.substr(0, name.rfind(">("));
			if (name.empty() || name.front() == '(') return {};  // "(enum koi::X)0x5": no enumerator with that value
			if (const size_t colon = name.rfind("::"); colon != std::string_view::npos) name.remove_prefix(colon + 2);
			return name;
		}

		template <typename E, size_t Max>
		constexpr auto EnumProbeAll()
		{
			return []<size_t... I>(std::index_sequence<I...>)
			{
				return std::array<std::string_view, Max>{ EnumProbe<static_cast<E>(I)>()... };
			}(std::make_index_sequence<Max>{});
		}

		// Names are copied out of the probes' signatures into one null-separated buffer
		template <typename E, size_t Max>
		struct EnumTable
		{
			static constexpr size_t kCount = []
			{
				size_t count = 0;
				for (std::string_view name : EnumProbeAll<E, Max>()) count += name.empty() ? 0 : 1;
				return count;
			}();

			static constexpr size_t kChars = []
			{
				size_t chars = 0;
				for (std::string_view name : EnumProbeAll<E, Max>()) chars += name.empty() ? 0 : name.size() + 1;
				return chars;
			}();

			static constexpr std::array<char, kChars + 1> kStorage = []
			{
				std::array<char, kChars + 1> out{};
				size_t at = 0;
				for (std::string_view name : EnumProbeAll<E, Max>())
				{
					if (name.empty()) continue;
					for (const char c : name)
					{
						out[at++] = c;
					}
					out[at++] = '\0';
				}
				return out;
			}();

			static constexpr std::array<EnumEntry<E>, kCount> kEntries = []
			{
				std::array<EnumEntry<E>, kCount> out{};
				const auto names = EnumProbeAll<E, Max>();
				size_t at = 0, n = 0;
				for (size_t i = 0; i < Max; ++i)
				{
					if (names[i].empty()) continue;
					out[n++] = { std::string_view(kStorage.data() + at, names[i].size()), static_cast<E>(i) };
					at += names[i].size() + 1;
				}
				return out;
			}();
		};

		template <typename E, size_t Max>
		consteval size_t EnumProbeRange()
		{
			using U = std::underlying_type_t<E>;
			constexpr size_t limit = static_cast<size_t>(std::numeric_limits<U>::max()) + 1;
			return Max < limit ? Max : limit;
		}
	}

	// fn(const FieldInfo&, field&) for each field in declaration order
	template <typename T, typename Fn> requires Reflected<std::remove_cvref_t<T>>
	void ForEachField(T&& obj, Fn&& fn)
	{
		using R = std::remove_cvref_t<T>;
		const auto& infos = internal::FieldInfos<R>();
		[&]<size_t... I>(std::index_sequence<I...>)
		{
			(fn(infos[I], obj.*(std::get<I>(kFields<R>).Member)), ...);
		}(std::make_index_sequence<kFieldCount<R>>{});
	}

	// fn(const FieldInfo&, member pointer), for walking several objects side by side
	template <Reflected T, typename Fn>
	void ForEachMember(Fn&& fn)
	{
		const auto& infos = internal::FieldInfos<T>();
		[&]<size_t... I>(std::index_sequence<I...>)
		{
			(fn(infos[I], std::get<I>(kFields<T>).Member), ...);
		}(std::make_index_sequence<kFieldCount<T>>{});
	}

	// Every named value in [0, Max)
	template <typename E, size_t Max = 64> requires std::is_enum_v<E>
	constexpr std::span<const EnumEntry<E>> EnumEntries()
	{
		return internal::EnumTable<E, internal::EnumProbeRange<E, Max>()>::kEntries;
	}

	// Empty for a value without a name
	template <typename E, size_t Max = 64> requires std::is_enum_v<E>
	constexpr std::string_view EnumName(E value)
	{
		for (const EnumEntry<E>& entry : EnumEntries<E, Max>())
		{
			if (entry.Value == value) return entry.Name;
		}
		return {};
	}

	template <typename E, size_t Max = 64> requires std::is_enum_v<E>
	constexpr std::optional<E> EnumFromName(std::string_view name)
	{
		for (const EnumEntry<E>& entry : EnumEntries<E, Max>())
		{
			if (entry.Name == name) return entry.Value;
		}
		return std::nullopt;
	}
}
