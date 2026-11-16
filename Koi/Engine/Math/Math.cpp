#include "Koi/Engine/Math/Math.h"
#include <cmath>

namespace koi
{
	namespace
	{
		bool IsUsable(const Quaternion& q)
		{
			const f32 lengthSq = q.LengthSquared();
			return std::isfinite(lengthSq) && lengthSq > 1e-12f;
		}
	}

	void math::MergeBounds(BoundingBox& inOutBox, const BoundingBox& other)
	{
		if (IsEmpty(other)) return;

		if (IsEmpty(inOutBox))
		{
			inOutBox = other;
		}
		else
		{
			BoundingBox::CreateMerged(inOutBox, inOutBox, other);
		}
	}

	f32 math::BoundsRadius(const BoundingBox& b)
	{
		BoundingSphere sphere;
		BoundingSphere::CreateFromBoundingBox(sphere, b);
		return sphere.Radius > 0.001f ? sphere.Radius : 0.001f;
	}

	Quaternion math::SafeNormalize(const Quaternion& q)
	{
		if (!IsUsable(q)) return Quaternion::Identity;

		Quaternion result;
		q.Normalize(result);
		return result;
	}

	bool math::IsSingular(const Matrix& m)
	{
		const f32 det = m.Determinant();
		return !std::isfinite(det) || std::abs(det) < 1e-12f;
	}

	CullVolume::CullVolume(const BoundingFrustum& frustum)
		: m_frustum(frustum)
	{
		const Quaternion orientation(frustum.Orientation);
		if (!IsUsable(orientation)) return;

		m_frustum.Orientation = math::SafeNormalize(orientation);
		m_kind = Kind::Frustum;
	}

	CullVolume::CullVolume(const BoundingOrientedBox& box)
		: m_box(box)
	{
		const Quaternion orientation(box.Orientation);
		if (!IsUsable(orientation)) return;

		m_box.Orientation = math::SafeNormalize(orientation);
		m_kind = Kind::Box;
	}

	CullVolume CullVolume::FromCamera(const Matrix& view, const Matrix& projection)
	{
		// CreateFromMatrix builds the frustum in view space, then the inverse view puts it in the world
		BoundingFrustum frustum;
		BoundingFrustum::CreateFromMatrix(frustum, projection, true /*rhcoords*/);

		const Matrix invView = view.Invert();
		frustum.Transform(frustum, DirectX::XMLoadFloat4x4(&invView));

		return CullVolume(frustum);
	}

	bool CullVolume::Intersects(const BoundingBox& worldBounds) const
	{
		switch (m_kind)
		{
			case Kind::Frustum: return m_frustum.Intersects(worldBounds);
			case Kind::Box:     return m_box.Intersects(worldBounds);
			case Kind::None:    break;
		}
		return true;
	}
}
