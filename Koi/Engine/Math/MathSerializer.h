#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Runtime/Serializer.h"

namespace koi
{
	template<>
	struct Serializer<float2>
	{
		static toml::array Serialize(const float2& f)
		{
			interop::float2 f2(f);
			return toml::array{ f2.x, f2.y };
		}

		static std::optional<float2> Deserialize(const toml::node& node)
		{
			if (const toml::array* arr = node.as_array(); arr->size() == 2)
			{
				return float2(
					(*arr)[0].value_or<f32>(0.0f),
					(*arr)[1].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	template<>
	struct Serializer<float3>
	{
		static toml::array Serialize(const float3& f)
		{
			interop::float3 f3(f);
			return toml::array{ f3.x, f3.y, f3.z };
		}

		static std::optional<float3> Deserialize(const toml::node& node)
		{
			if (const toml::array* asArr = node.as_array(); asArr->size() == 3)
			{
				const toml::array& arr = *asArr;
				return float3(
					arr[0].value_or<f32>(0.0f),
					arr[1].value_or<f32>(0.0f),
					arr[2].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	template<>
	struct Serializer<float4>
	{
		static toml::array Serialize(const float4& f)
		{
			interop::float4 f4(f);
			return toml::array{ f4.x, f4.y, f4.z, f4.w };
		}

		static std::optional<float4> Deserialize(const toml::node& node)
		{
			if (const toml::array* arr = node.as_array(); arr->size() == 4)
			{
				return float4(
					(*arr)[0].value_or<f32>(0.0f),
					(*arr)[1].value_or<f32>(0.0f),
					(*arr)[2].value_or<f32>(0.0f),
					(*arr)[3].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	template<>
	struct Serializer<quaternion>
	{
		static toml::array Serialize(const quaternion& quat)
		{
			interop::float4 q(quat.xyzw);
			return toml::array{ q.x, q.y, q.z, q.w };
		}

		static std::optional<quaternion> Deserialize(const toml::node& node)
		{
			if (const toml::array* arr = node.as_array(); arr->size() == 4)
			{
				return quaternion(
					(*arr)[0].value_or<f32>(0.0f),
					(*arr)[1].value_or<f32>(0.0f),
					(*arr)[2].value_or<f32>(0.0f),
					(*arr)[3].value_or<f32>(0.0f));
			}
			return std::nullopt;
		}
	};

	static_assert(Serializable<float2>);
	static_assert(Serializable<float3>);
	static_assert(Serializable<float4>);
	static_assert(Serializable<quaternion>);
}
