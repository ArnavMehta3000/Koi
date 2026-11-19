#pragma once
#include "Koi/Engine/Core/Types.h"

#include <toml++/toml.hpp>

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
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

	// f32 needs its own: the default above would round-trip through value_exact<f32>, which toml has no type for
	template <>
	struct Serializer<f32>
	{
		static f64 Serialize(f32 value) { return static_cast<f64>(value); }
		static std::optional<f32> Deserialize(const toml::node& node) { return node.value<f32>(); }
	};

	namespace utils
	{
		inline constexpr toml::format_flags kTomlFormatFlags =
			toml::toml_formatter::default_flags | toml::format_flags::relaxed_float_precision;

		[[nodiscard]] inline toml::toml_formatter TomlDocument(const toml::node& node)
		{
			return toml::toml_formatter(node, kTomlFormatFlags);
		}

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
}
