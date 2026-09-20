#pragma once
#include "Koi/Engine/Math/Ease.h"
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Runtime/Serializer.h"

namespace koi
{
	template<>
	struct EnumNames<EaseType>
	{
		static std::string_view ToString(EaseType value)                 { return koi::ToString(value); }
		static std::optional<EaseType> FromString(std::string_view name) { return EaseTypeFromString(name); }
	};

	template<>
	struct Serializer<Vector2>
	{
		static toml::array Serialize(const Vector2& f)
		{
			return toml::array{ ToTomlFloat(f.x), ToTomlFloat(f.y) };
		}

		static std::optional<Vector2> Deserialize(const toml::node& node)
		{
			if (const toml::array* asArr = node.as_array(); asArr && asArr->size() == 2)
			{
				const toml::array& arr = *asArr;
				return Vector2(
					arr[0].value_or<f32>(0.0f),
					arr[1].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	template<>
	struct Serializer<Vector3>
	{
		static toml::array Serialize(const Vector3& f)
		{
			return toml::array{ ToTomlFloat(f.x), ToTomlFloat(f.y), ToTomlFloat(f.z) };
		}

		static std::optional<Vector3> Deserialize(const toml::node& node)
		{
			if (const toml::array* asArr = node.as_array(); asArr && asArr->size() == 3)
			{
				const toml::array& arr = *asArr;
				return Vector3(
					arr[0].value_or<f32>(0.0f),
					arr[1].value_or<f32>(0.0f),
					arr[2].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	template<>
	struct Serializer<Vector4>
	{
		static toml::array Serialize(const Vector4& f)
		{
			return toml::array{ ToTomlFloat(f.x), ToTomlFloat(f.y), ToTomlFloat(f.z), ToTomlFloat(f.w) };
		}

		static std::optional<Vector4> Deserialize(const toml::node& node)
		{
			if (const toml::array* asArr = node.as_array(); asArr && asArr->size() == 4)
			{
				const toml::array& arr = *asArr;
				return Vector4(
					arr[0].value_or<f32>(0.0f),
					arr[1].value_or<f32>(0.0f),
					arr[2].value_or<f32>(0.0f),
					arr[3].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	template<>
	struct Serializer<Quaternion>
	{
		static toml::array Serialize(const Quaternion& q)
		{
			return toml::array{ ToTomlFloat(q.x), ToTomlFloat(q.y), ToTomlFloat(q.z), ToTomlFloat(q.w) };
		}

		static std::optional<Quaternion> Deserialize(const toml::node& node)
		{
			if (const toml::array* asArr = node.as_array(); asArr && asArr->size() == 4)
			{
				const toml::array& arr = *asArr;
				return Quaternion(
					arr[0].value_or<f32>(0.0f),
					arr[1].value_or<f32>(0.0f),
					arr[2].value_or<f32>(0.0f),
					arr[3].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	static_assert(Serializable<Vector2>);
	static_assert(Serializable<Vector3>);
	static_assert(Serializable<Vector4>);
	static_assert(Serializable<Quaternion>);
	static_assert(Serializable<EaseType>);
}
