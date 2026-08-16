#include "Koi/Engine/Math/Math.h"

namespace koi
{
	void MergeBounds(BoundingBox& inOutBox, const BoundingBox& other)
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

	f32 BoundsRadius(const BoundingBox& b)
	{
		BoundingSphere sphere;
		BoundingSphere::CreateFromBoundingBox(sphere, b);
		return sphere.Radius > 0.001f ? sphere.Radius : 0.001f;
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
