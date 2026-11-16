#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace koi
{
	namespace
	{
		u32 PackColor(const Color& c)
		{
			auto channel = [](f32 v) { return static_cast<u32>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
			return channel(c.x) | (channel(c.y) << 8) | (channel(c.z) << 16) | (channel(c.w) << 24);
		}

		// The eight NDC corners, near ring first, in the order BoxCorners expects
		constexpr Vector3 kClipCorners[8] =
		{
			{ -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f },
			{ -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f },
		};
	}

	void DebugDrawQueue::Line(const Vector3& a, const Vector3& b, const Color& color)
	{
		const u32 packed = PackColor(color);
		m_vertices.push_back(DebugVertex{ a, packed });
		m_vertices.push_back(DebugVertex{ b, packed });
	}

	void DebugDrawQueue::BoxCorners(std::span<const Vector3, 8> c, const Color& color)
	{
		for (u32 i = 0; i < 4; ++i)
		{
			const u32 next = (i + 1) % 4;
			Line(c[i], c[next], color);
			Line(c[i + 4], c[next + 4], color);
			Line(c[i], c[i + 4], color);
		}
	}

	void DebugDrawQueue::Box(const BoundingBox& box, const Color& color)
	{
		// DirectX corner order is the +Z ring then the -Z ring, which BoxCorners already matches
		std::array<Vector3, 8> corners{};
		box.GetCorners(reinterpret_cast<DirectX::XMFLOAT3*>(corners.data()));
		BoxCorners(corners, color);
	}

	void DebugDrawQueue::Box(const BoundingOrientedBox& box, const Color& color)
	{
		BoundingOrientedBox safe = box;
		safe.Orientation = math::SafeNormalize(Quaternion(box.Orientation));

		std::array<Vector3, 8> corners{};
		safe.GetCorners(reinterpret_cast<DirectX::XMFLOAT3*>(corners.data()));
		BoxCorners(corners, color);
	}

	// Corners pushed through the matrix, so scale and shear never go through a quaternion
	void DebugDrawQueue::Box(const BoundingBox& local, const Matrix& world, const Color& color)
	{
		std::array<Vector3, 8> corners{};
		local.GetCorners(reinterpret_cast<DirectX::XMFLOAT3*>(corners.data()));
		for (Vector3& corner : corners)
		{
			corner = Vector3::Transform(corner, world);
		}
		BoxCorners(corners, color);
	}

	void DebugDrawQueue::Sphere(const Vector3& center, f32 radius, const Color& color)
	{
		constexpr u32 kSegments = 32;
		constexpr f32 kStep     = DirectX::XM_2PI / kSegments;

		for (u32 i = 0; i < kSegments; ++i)
		{
			const f32 a = i * kStep;
			const f32 b = (i + 1) * kStep;

			const Vector2 p0(std::cos(a) * radius, std::sin(a) * radius);
			const Vector2 p1(std::cos(b) * radius, std::sin(b) * radius);

			Line(center + Vector3(p0.x, p0.y, 0.0f), center + Vector3(p1.x, p1.y, 0.0f), color);
			Line(center + Vector3(p0.x, 0.0f, p0.y), center + Vector3(p1.x, 0.0f, p1.y), color);
			Line(center + Vector3(0.0f, p0.x, p0.y), center + Vector3(0.0f, p1.x, p1.y), color);
		}
	}

	void DebugDrawQueue::Capsule(const Vector3& a, const Vector3& b, f32 radius, const Color& color)
	{
		Vector3 axis = b - a;
		if (axis.LengthSquared() < 1e-8f)
		{
			Sphere(a, radius, color);
			return;
		}
		axis.Normalize();

		// Any two directions perpendicular to the axis
		const Vector3 helper = std::abs(axis.y) < 0.99f ? Vector3::UnitY : Vector3::UnitX;
		Vector3 side = axis.Cross(helper);
		side.Normalize();
		const Vector3 other = axis.Cross(side);

		Sphere(a, radius, color);
		Sphere(b, radius, color);
		for (const Vector3& offset : { side, -side, other, -other })
		{
			Line(a + offset * radius, b + offset * radius, color);
		}
	}

	void DebugDrawQueue::Frustum(const Matrix& viewProjection, const Color& color)
	{
		const Matrix inverse = viewProjection.Invert();

		std::array<Vector3, 8> corners{};
		for (u32 i = 0; i < 8; ++i)
		{
			const Vector4 h = Vector4::Transform(Vector4(kClipCorners[i].x, kClipCorners[i].y, kClipCorners[i].z, 1.0f), inverse);
			corners[i] = Vector3(h.x, h.y, h.z) / h.w;
		}
		BoxCorners(corners, color);
	}

	void DebugDrawQueue::Axes(const Transform& transform, f32 length)
	{
		const Matrix m = Matrix::CreateFromQuaternion(transform.Rotation);
		const Vector3 origin = transform.Position;

		Line(origin, origin + m.Right()   * length, Color(1.0f, 0.0f, 0.0f, 1.0f));
		Line(origin, origin + m.Up()      * length, Color(0.0f, 1.0f, 0.0f, 1.0f));
		Line(origin, origin + m.Forward() * length, Color(0.0f, 0.0f, 1.0f, 1.0f));
	}

	DebugDrawQueue& DebugDraw::Queue()
	{
		return Engine::Get().GetSubsystem<Renderer>().GetDebugDraw();
	}
}
