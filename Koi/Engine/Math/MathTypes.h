
#pragma once
#include "Koi/Engine/Core/Types.h"
#include <SimpleMath.h>

namespace koi
{
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

	using BoundingBox           = DirectX::BoundingBox;
	using BoundingOrientedBox   = DirectX::BoundingOrientedBox;
	using BoundingSphere        = DirectX::BoundingSphere;
	using BoundingFrustum       = DirectX::BoundingFrustum;
	using ContainmentType       = DirectX::ContainmentType;
	using PlaneIntersectionType = DirectX::PlaneIntersectionType;
}
