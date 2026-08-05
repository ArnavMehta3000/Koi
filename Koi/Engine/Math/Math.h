// DirectXTK SimpleMath types + engine math helpers
#pragma once

#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Core/Types.h"

#include <SimpleMath.h>

namespace koi
{
	class World;

	using Vector2    = DirectX::SimpleMath::Vector2;
	using Vector3    = DirectX::SimpleMath::Vector3;
	using Vector4    = DirectX::SimpleMath::Vector4;
	using Matrix     = DirectX::SimpleMath::Matrix;
	using Quaternion = DirectX::SimpleMath::Quaternion;
	using Rectangle  = DirectX::SimpleMath::Rectangle;
	using Plane      = DirectX::SimpleMath::Plane;
	using Ray        = DirectX::SimpleMath::Ray;
	using Color      = DirectX::SimpleMath::Color;
	using Viewport   = DirectX::SimpleMath::Viewport;

	struct AABB
	{
		Vector3 Min;
		Vector3 Max;

		void Expand(const Vector3& point);
		void Expand(const AABB& other);

		[[nodiscard]] Vector3 Center() const;

		// Radius of a sphere that encloses this AABB, centered at Center()
		[[nodiscard]] f32 Radius() const;
	};

	class MathUtils
	{
	public:
		// Aligns a value to the nearest multiple of 256
		template <typename T>
		static inline T AlignGPU256(T value) { return (value + static_cast<T>(255)) & ~static_cast<T>(255); }
	};
}
