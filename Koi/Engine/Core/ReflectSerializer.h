#pragma once
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/MathSerializer.h"
#include <algorithm>
#include <array>

namespace koi
{
	namespace internal
	{
		template <typename T> inline constexpr bool kIsStdArray = false;
		template <typename T, size_t N> inline constexpr bool kIsStdArray<std::array<T, N>> = true;

		template <typename F>
		auto ReflectToToml(const F& value)
		{
			if constexpr (std::is_enum_v<F>)
				return std::string(refl::EnumName(value));
			else if constexpr (std::integral<F> && !std::same_as<F, bool>)
				return static_cast<i64>(value);
			else if constexpr (kIsStdArray<F>)
			{
				toml::array out;
				for (const auto& element : value)
				{
					out.push_back(ReflectToToml(element));
				}
				return out;
			}
			else
			{
				return Serializer<F>::Serialize(value);
			}
		}

		template <typename F>
		std::optional<F> ReflectFromToml(const toml::node& node)
		{
			if constexpr (std::is_enum_v<F>)
			{
				const std::optional<std::string_view> name = node.value<std::string_view>();
				return name ? refl::EnumFromName<F>(*name) : std::nullopt;
			}
			else if constexpr (std::integral<F> && !std::same_as<F, bool>)
			{
				return node.value<F>();
			}
			else if constexpr (kIsStdArray<F>)
			{
				const toml::array* arr = node.as_array();
				if (!arr || arr->size() != std::tuple_size_v<F>)
					return std::nullopt;

				F out{};
				for (size_t i = 0; i < out.size(); ++i)
				{
					auto element = ReflectFromToml<typename F::value_type>(*arr->get(i));
					if (!element) return std::nullopt;
					out[i] = std::move(*element);
				}
				return out;
			}
			else
				return Serializer<F>::Deserialize(node);
		}

		template <refl::Reflected T>
		void WarnUnknownKeys(const toml::table& table)
		{
			const auto& fields = refl::internal::FieldInfos<T>();
			for (const auto& [key, value] : table)
			{
				const bool known = std::ranges::any_of(fields, [&](const refl::FieldInfo& field)
				{
					return !field.IsTransient && field.Key == key.str();
				});

				if (!known)
				{
					KOI_LOG(Scene, Warn, "'{}' has unknown key '{}'; it will be dropped on save", refl::NameOf<T>(), key.str());
				}
			}
		}
	}

	template <refl::Reflected T>
	struct Serializer<T>
	{
		static toml::table Serialize(const T& value)
		{
			toml::table out;
			refl::ForEachField(value, [&]<typename F>(const refl::FieldInfo& field, const F& member)
			{
				if constexpr (!refl::kIsTransient<F>)
				{
					out.insert_or_assign(field.Key, internal::ReflectToToml(member));
				}
			});
			return out;
		}

		static std::optional<T> Deserialize(const toml::node& node)
		{
			const toml::table* table = node.as_table();
			if (!table)
			{
				KOI_LOG(Scene, Error, "'{}' failed to load: expected a table", refl::NameOf<T>());
				return std::nullopt;
			}
			internal::WarnUnknownKeys<T>(*table);

			T out{};
			refl::ForEachField(out, [&]<typename F>(const refl::FieldInfo& field, F& member)
			{
				if constexpr (!refl::kIsTransient<F>)
				{
					const toml::node* child = table->get(field.Key);
					if (!child) return;

					if (std::optional<F> loaded = internal::ReflectFromToml<F>(*child))
					{
						member = std::move(*loaded);
					}
					else
					{
						KOI_LOG(Scene, Warn, "'{}' has an invalid '{}'; keeping the default", refl::NameOf<T>(), field.Key);
					}
				}
			});
			return out;
		}
	};
}
