#pragma once
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Content/Shaders/Interop/DebugDraw.hlsli"
#include "Koi/Engine/Math/Transform.h"
#include <span>
#include <vector>

namespace koi
{
	class DebugDrawQueue
	{
	public:
		void Line(const Vector3& a, const Vector3& b, const Color& color);
		void Box(const BoundingBox& box, const Color& color);
		void Box(const BoundingOrientedBox& box, const Color& color);
		void Box(const BoundingBox& local, const Matrix& world, const Color& color);
		void Sphere(const Vector3& center, f32 radius, const Color& color);
		void Capsule(const Vector3& a, const Vector3& b, f32 radius, const Color& color);
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

#if KOI_DEBUG_DRAW
		inline void Line(const Vector3& a, const Vector3& b, const Color& color)   { Queue().Line(a, b, color); }
		inline void Box(const BoundingBox& box, const Color& color)                { Queue().Box(box, color); }
		inline void Box(const BoundingOrientedBox& box, const Color& color)        { Queue().Box(box, color); }
		inline void Box(const BoundingBox& local, const Matrix& world, const Color& color) { Queue().Box(local, world, color); }
		inline void Sphere(const Vector3& c, f32 radius, const Color& color)       { Queue().Sphere(c, radius, color); }
		inline void Capsule(const Vector3& a, const Vector3& b, f32 radius, const Color& color) { Queue().Capsule(a, b, radius, color); }
		inline void Frustum(const Matrix& viewProjection, const Color& color)      { Queue().Frustum(viewProjection, color); }
		inline void Axes(const Transform& transform, f32 length = 1.0f)            { Queue().Axes(transform, length); }
#else
		inline void Line(const Vector3&, const Vector3&, const Color&)             {}
		inline void Box(const BoundingBox&, const Color&)                          {}
		inline void Box(const BoundingOrientedBox&, const Color&)                  {}
		inline void Box(const BoundingBox&, const Matrix&, const Color&)           {}
		inline void Sphere(const Vector3&, f32, const Color&)                      {}
		inline void Capsule(const Vector3&, const Vector3&, f32, const Color&)     {}
		inline void Frustum(const Matrix&, const Color&)                           {}
		inline void Axes(const Transform&, f32 = 1.0f)                             {}
#endif
	}
}
