#pragma once
#include "Koi/Engine/Math/MathSerializer.h"
#include "Koi/Engine/Logging/Logger.h"
#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <algorithm>
#include <array>
#include <cctype>

// A component opts in with `static constexpr bool kReflect = true` and gets Serializer<T> for free
namespace rfl
{
	template <koi::FloatArray T>
	struct Reflector<T>
	{
		using ReflType = std::array<koi::f32, koi::kFloatCount<T>>;

		static ReflType from(const T& value)
		{
			ReflType out{};
			std::copy_n(&value.x, out.size(), out.begin());
			return out;
		}

		static T to(const ReflType& flat)
		{
			T out{};
			std::copy_n(flat.begin(), flat.size(), &out.x);
			return out;
		}
	};
}

namespace koi
{
	template <typename T>
	concept ReflectedComponent = requires { { T::kReflect } -> std::convertible_to<bool>; } && T::kReflect;

	using ReflectProcessors = rfl::Processors<rfl::CamelCaseToSnakeCase, rfl::DefaultIfMissing>;

	namespace internal
	{
		// "PopDuration" -> "pop_duration", the same mapping CamelCaseToSnakeCase applies at compile time
		inline std::string SnakeCase(std::string_view name)
		{
			const auto size = name.size();
			std::string out;
			out.reserve(size);  // Reserve the same amount of bytes as the name (might be more since '_' will be added)

			for (size_t i = 0; i < size; ++i)
			{
				const unsigned char c = static_cast<unsigned char>(name[i]);
				if (i > 0 && std::isupper(c))
					out += '_';

				out += static_cast<char>(std::tolower(c));
			}
			return out;
		}

		// A renamed or misspelled key would otherwise be silently replaced by the field default
		template <typename T>
		void WarnUnknownKeys(const toml::table& table)
		{
			static const std::vector<std::string> known = []
			{
				std::vector<std::string> names;
				for (const rfl::MetaField& field : rfl::fields<T>())
				{
					names.push_back(SnakeCase(field.name()));
				}
				return names;
			}();

			for (const auto& [key, value] : table)
			{
				if (!std::ranges::contains(known, key.str()))
				{
					KOI_LOG_WARN("Component '{}' has unknown key '{}'; it will be dropped on save", T::kKey, key.str());
				}
			}
		}
	}

	template <ReflectedComponent T>
	struct Serializer<T>
	{
		static toml::table Serialize(const T& value)
		{
			toml::table out;
			rfl::toml::Writer writer(&out);
			rfl::toml::Parser<T, ReflectProcessors>::write(writer, value, typename rfl::parsing::Parent<rfl::toml::Writer>::Root{});
			return out;
		}

		static std::optional<T> Deserialize(const toml::node& node)
		{
			if (const toml::table* table = node.as_table()) internal::WarnUnknownKeys<T>(*table);

			auto result = rfl::toml::read<T, rfl::CamelCaseToSnakeCase, rfl::DefaultIfMissing>(const_cast<toml::node*>(&node));
			if (!result)
			{
				KOI_LOG_ERROR("Component '{}' failed to load: {}", T::kKey, result.error().what());
				return std::nullopt;
			}
			return std::move(*result);
		}
	};
}
