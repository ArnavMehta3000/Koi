#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Math/Transform.h"
#include <span>
#include <vector>

namespace koi
{
	struct DebugVertex
	{
		Vector3 Position;
		u32     Color;  // RGBA8
	};

	class DebugDrawQueue
	{
	public:
		void Line(const Vector3& a, const Vector3& b, const Color& color);
		void Box(const BoundingBox& box, const Color& color);
		void Box(const BoundingOrientedBox& box, const Color& color);
		void Sphere(const Vector3& center, f32 radius, const Color& color);
		void Frustum(const Matrix& viewProjection, const Color& color);
		void Axes(const Transform& transform, f32 length = 1.0f);

		inline void Clear() { m_vertices.clear(); }
		[[nodiscard]] inline std::span<const DebugVertex> GetVertices() const { return m_vertices; }

	private:
		// Ring 0-1-2-3, ring 4-5-6-7, and the four edges between them
		void BoxCorners(std::span<const Vector3, 8> corners, const Color& color);

	private:
		std::vector<DebugVertex> m_vertices;
	};

	namespace DebugDraw
	{
		[[nodiscard]] DebugDrawQueue& Queue();

		inline void Line(const Vector3& a, const Vector3& b, const Color& color)   { Queue().Line(a, b, color); }
		inline void Box(const BoundingBox& box, const Color& color)                { Queue().Box(box, color); }
		inline void Box(const BoundingOrientedBox& box, const Color& color)        { Queue().Box(box, color); }
		inline void Sphere(const Vector3& c, f32 radius, const Color& color)       { Queue().Sphere(c, radius, color); }
		inline void Frustum(const Matrix& viewProjection, const Color& color)      { Queue().Frustum(viewProjection, color); }
		inline void Axes(const Transform& transform, f32 length = 1.0f)            { Queue().Axes(transform, length); }
	}
}
