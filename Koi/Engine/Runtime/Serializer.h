#pragma once

#include <toml++/toml.hpp>

#include <concepts>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace koi
{
	template <typename T>
	struct Serializer;

	// Types toml++ stores directly, without a table or array wrapper.
	template <typename T>
	concept TomlNative = std::same_as<T, bool>
		|| (std::integral<T>
			&& !std::same_as<T, bool>
			&& !std::same_as<T, char>
			&& !std::same_as<T, wchar_t>
			&& !std::same_as<T, char8_t>
			&& !std::same_as<T, char16_t>
			&& !std::same_as<T, char32_t>)
		|| std::floating_point<T>
		|| std::same_as<T, std::string>
		|| std::same_as<T, toml::date>
		|| std::same_as<T, toml::time>
		|| std::same_as<T, toml::date_time>;

	// Anything Serialize may hand back: a native, or an owning node
	template <typename T>
	concept TomlInsertable = TomlNative<T> || std::derived_from<T, toml::node>;


	// --- For enums ---
	template <typename E>
	struct EnumNames;

	template <typename E>
	concept NamedEnum = std::is_enum_v<E> && requires(E value, std::string_view name)
	{
		{ EnumNames<E>::ToString(value) } -> std::convertible_to<std::string_view>;
		{ EnumNames<E>::FromString(name) } -> std::same_as<std::optional<E>>;
	};

	// Maps with a string-like key, which round-trip as a toml::table.
	template <typename T>
	concept StringKeyedMap = requires
	{
		typename T::key_type;
		typename T::mapped_type;
	}&& std::convertible_to<typename T::key_type, std::string_view>;

	// Ranges we can rebuild element-by-element (vector, list, deque, set...).
	// Fixed-extent ranges (std::array, std::span, T[N]) need their own
	// Serializer specialization.
	template <typename R>
	concept RebuildableRange =
		std::ranges::input_range<R>
		&& std::default_initializable<R>
		&& requires(R & range, std::ranges::range_value_t<R> && elem)
	{
		range.insert(range.end(), std::move(elem));
	};


	template <typename T>
	concept CustomSerializable = requires(const T & value)
	{
		{ Serializer<T>::Serialize(value) } -> TomlInsertable;
	};

	template <typename T>
	concept CustomDeserializable = requires(const toml::node & node)
	{
		{ Serializer<T>::Deserialize(node) } -> std::same_as<std::optional<T>>;
	};

	template <typename T>
	concept Serializable = CustomSerializable<T> && CustomDeserializable<T>;

	// Default native types
	template <TomlNative T>
	struct Serializer<T>
	{
		static T Serialize(const T& value) { return value; }
		static std::optional<T> Deserialize(const toml::node& node) { return node.value_exact<T>(); }
	};

	// Enums
	template <NamedEnum E>
	struct Serializer<E>
	{
		static std::string Serialize(E value)
		{
			return std::string{ EnumNames<E>::ToString(value) };
		}

		static std::optional<E> Deserialize(const toml::node& node)
		{
			const auto name = node.value<std::string_view>();
			return name ? EnumNames<E>::FromString(*name) : std::nullopt;
		}
	};

	// Ranges -> toml::array. std::string is excluded via TomlNative, maps take the specialization below
	template <RebuildableRange R> requires
		(!TomlNative<R>)
		&& (!StringKeyedMap<R>)
		&& Serializable<std::ranges::range_value_t<R>>
	struct Serializer<R>
	{
		using Elem = std::ranges::range_value_t<R>;

		static toml::array Serialize(const R& range)
		{
			toml::array out;
			for (const auto& elem : range)
			{
				out.push_back(Serializer<Elem>::Serialize(elem));
			}

			return out;
		}

		static std::optional<R> Deserialize(const toml::node& node)
		{
			const auto* arr = node.as_array();
			if (!arr) return std::nullopt;

			R out{};
			for (const auto& child : *arr)
			{
				auto elem = Serializer<Elem>::Deserialize(child);
				if (!elem) return std::nullopt;

				out.insert(out.end(), std::move(*elem));
			}

			return out;
		}
	};

	// For map types where key is std::string -> toml::table
	template <StringKeyedMap M>
		requires Serializable<typename M::mapped_type>
	struct Serializer<M>
	{
		using Mapped = typename M::mapped_type;

		static toml::table Serialize(const M& map)
		{
			toml::table out;
			for (const auto& [key, value] : map)
			{
				out.insert_or_assign(std::string_view{ key }, Serializer<Mapped>::Serialize(value));
			}

			return out;
		}

		static std::optional<M> Deserialize(const toml::node& node)
		{
			const auto* table = node.as_table();
			if (!table) return std::nullopt;

			M out{};
			for (const auto& [key, child] : *table)
			{
				auto value = Serializer<Mapped>::Deserialize(child);
				if (!value) return std::nullopt;

				out.emplace(typename M::key_type{ key.str() }, std::move(*value));
			}

			return out;
		}
	};

	// --- Helpers ---

	template <typename T>
	void SerializeInto(toml::table&, std::string_view, const T&)
	{
		static_assert(CustomSerializable<T>,
			"No koi::Serializer<T> specialization for this type, or its Serialize "
			"does not return a TOML native / owning node by value.");
	}

	template <CustomSerializable T>
	void SerializeInto(toml::table& table, std::string_view key, const T& value) 
	{
		table.insert_or_assign(key, Serializer<T>::Serialize(value));
	}

	// Fails object creation if the key is missing
	template <CustomDeserializable T>
	std::optional<T> DeserializeFrom(const toml::table& table, std::string_view key)
	{
		const auto* node = table.get(key);
		if (!node)
			return std::nullopt;

		return Serializer<T>::Deserialize(*node);
	}

	// Set the fallback default if the key is missing
	template <CustomDeserializable T>
	T DeserializeOr(const toml::table& table, std::string_view key, T fallback)
	{
		auto value = DeserializeFrom<T>(table, key);
		return value ? std::move(*value) : std::move(fallback);
	}
}

//-------------------------------------------------------------------------------
// Example specialization
//
//   template <>
//   struct koi::Serializer<Transform>
//   {
//       static toml::table Serialize(const Transform& v)
//       {
//           toml::table out;
//           SerializeInto(out, "position", v.position);
//           SerializeInto(out, "rotation", v.rotation);
//           SerializeInto(out, "scale",    v.scale);
//           return out;
//       }
//
//       static std::optional<Transform> Deserialize(const toml::node& node)
//       {
//           const auto* table = node.as_table();
//           if (!table)
//               return std::nullopt;
//
//           Transform out{};
//           out.position = DeserializeOr(*table, "position", math::float3(0.0f));
//           out.rotation = DeserializeOr(*table, "rotation", math::quaternion::identity());
//           out.scale    = DeserializeOr(*table, "scale",    math::float3(1.0f));
//           return out;
//       }
//   };
//
//	 template <>
//	 struct koi::EnumNames<CullMode>
//	 {
//	     static std::string_view ToString(CullMode value);
//	     static std::optional<CullMode> FromString(std::string_view name);
//	 };
//-------------------------------------------------------------------------------
