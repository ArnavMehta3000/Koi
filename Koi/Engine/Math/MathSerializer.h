#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Runtime/Serializer.h"

namespace koi
{
	template <typename T> inline constexpr size_t kFloatCount   = 0;
	template <> inline constexpr size_t kFloatCount<Vector2>    = 2;
	template <> inline constexpr size_t kFloatCount<Vector3>    = 3;
	template <> inline constexpr size_t kFloatCount<Vector4>    = 4;
	template <> inline constexpr size_t kFloatCount<Quaternion> = 4;

	template <typename T>
	concept FloatArray = kFloatCount<T> != 0;

	template <FloatArray T>
	struct Serializer<T>
	{
		static toml::array Serialize(const T& value)
		{
			const f32* components = &value.x;

			toml::array out;
			for (size_t i = 0; i < kFloatCount<T>; ++i)
			{
				out.push_back(components[i]);
			}
			return out;
		}

		static std::optional<T> Deserialize(const toml::node& node)
		{
			const toml::array* arr = node.as_array();
			if (!arr || arr->size() != kFloatCount<T>) return std::nullopt;

			T out{};
			f32* components = &out.x;
			for (size_t i = 0; i < kFloatCount<T>; ++i)
			{
				components[i] = (*arr)[i].value_or<f32>(0.0f);
			}
			return out;
		}
	};

	static_assert(Serializable<Vector2>);
	static_assert(Serializable<Vector3>);
	static_assert(Serializable<Vector4>);
	static_assert(Serializable<Quaternion>);
}
