// DirectXTK SimpleMath types + engine math helpers
#pragma once
#include "Koi/Engine/Math/MathTypes.h"

namespace koi
{
	namespace math
	{
		// Zero extents is the engine's "no bounds"
		[[nodiscard]] inline bool IsEmpty(const BoundingBox& b)
		{
			return b.Extents.x <= 0.0f && b.Extents.y <= 0.0f && b.Extents.z <= 0.0f;
		}

		// Merges into an accumulator that may still be empty
		void MergeBounds(BoundingBox& inOutBox, const BoundingBox& other);

		// Radius of the sphere enclosing the box
		[[nodiscard]] f32 BoundsRadius(const BoundingBox& b);

		// Aligns a value to the nearest multiple of 256
		template <typename T>
		[[nodiscard]] constexpr T AlignGPU256(T value) { return (value + static_cast<T>(255)) & ~static_cast<T>(255); }
	}

	// A camera frustum or a directional light's ortho box
	class CullVolume
	{
	public:
		CullVolume() = default;
		explicit CullVolume(const BoundingFrustum& frustum)  : m_kind(Kind::Frustum), m_frustum(frustum) {}
		explicit CullVolume(const BoundingOrientedBox& box)  : m_kind(Kind::Box),     m_box(box) {}

		// World-space frustum from a perspective camera. Ortho projections need the box form
		[[nodiscard]] static CullVolume FromCamera(const Matrix& view, const Matrix& projection);

		[[nodiscard]] bool Intersects(const BoundingBox& worldBounds) const;

	private:
		enum class Kind { None, Frustum, Box };

		Kind                m_kind = Kind::None;
		BoundingFrustum     m_frustum;
		BoundingOrientedBox m_box;
	};
}
