#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Serializer.h"
#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <algorithm>
#include <array>
#include <cctype>

// A component opts in with `static constexpr bool kReflect = true` and gets Serializer<T> for free
namespace rfl
{
	template <>
	struct Reflector<koi::Vector3>
	{
		using ReflType = std::array<koi::f32, 3>;
		static ReflType from(const koi::Vector3& v)  { return { v.x, v.y, v.z }; }
		static koi::Vector3 to(const ReflType& a)    { return { a[0], a[1], a[2] }; }
	};

	template <>
	struct Reflector<koi::Quaternion>
	{
		using ReflType = std::array<koi::f32, 4>;
		static ReflType from(const koi::Quaternion& q) { return { q.x, q.y, q.z, q.w }; }
		static koi::Quaternion to(const ReflType& a)   { return { a[0], a[1], a[2], a[3] }; }
	};
}

namespace koi
{
	template <typename T>
	concept ReflectedComponent = requires { { T::kReflect } -> std::convertible_to<bool>; } && T::kReflect;

	using ReflectProcessors = rfl::Processors<rfl::CamelCaseToSnakeCase, rfl::DefaultIfMissing>;

	// reflect-cpp widens f32 straight to double; re-shorten so files stay readable like ToTomlFloat does
	inline void ShortenFloats(toml::node& node)
	{
		if (toml::value<f64>* v = node.as_floating_point())
		{
			**v = ToTomlFloat(static_cast<f32>(**v));
		}
		else if (toml::table* t = node.as_table())
		{
			for (auto& [key, child] : *t) ShortenFloats(child);
		}
		else if (toml::array* a = node.as_array())
		{
			for (toml::node& child : *a) ShortenFloats(child);
		}
	}

	// "PopDuration" -> "pop_duration", the same mapping CamelCaseToSnakeCase applies at compile time
	inline std::string SnakeCase(std::string_view name)
	{
		std::string out;
		for (size_t i = 0; i < name.size(); ++i)
		{
			const unsigned char c = static_cast<unsigned char>(name[i]);
			if (i > 0 && std::isupper(c)) out += '_';
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

	template <ReflectedComponent T>
	struct Serializer<T>
	{
		static toml::table Serialize(const T& value)
		{
			toml::table out;
			rfl::toml::Writer writer(&out);
			rfl::toml::Parser<T, ReflectProcessors>::write(writer, value, typename rfl::parsing::Parent<rfl::toml::Writer>::Root{});
			ShortenFloats(out);
			return out;
		}

		static std::optional<T> Deserialize(const toml::node& node)
		{
			if (const toml::table* table = node.as_table()) WarnUnknownKeys<T>(*table);

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
