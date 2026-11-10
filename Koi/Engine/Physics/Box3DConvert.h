#pragma once
#include "Koi/Engine/Math/Transform.h"
#include <box3d/math_functions.h>

namespace koi::internal
{
	[[nodiscard]] inline b3Vec3 ToB3(const Vector3& v)    { return { v.x, v.y, v.z }; }
	[[nodiscard]] inline b3Quat ToB3(const Quaternion& q) { return { { q.x, q.y, q.z }, q.w }; }

	[[nodiscard]] inline Vector3    ToKoi(const b3Vec3& v) { return Vector3(v.x, v.y, v.z); }
	[[nodiscard]] inline Quaternion ToKoi(const b3Quat& q) { return Quaternion(q.v.x, q.v.y, q.v.z, q.s); }

#if defined(BOX3D_DOUBLE_PRECISION)
	[[nodiscard]] inline b3Pos   ToB3Pos(const Vector3& v) { return { v.x, v.y, v.z }; }
	[[nodiscard]] inline Vector3 ToKoi(const b3Pos& p)     { return Vector3(static_cast<f32>(p.x), static_cast<f32>(p.y), static_cast<f32>(p.z)); }
#else
	[[nodiscard]] inline b3Pos ToB3Pos(const Vector3& v) { return ToB3(v); }
#endif

	[[nodiscard]] inline b3WorldTransform ToB3(const Transform& xform)
	{
		return { ToB3Pos(xform.Position), ToB3(xform.Rotation) };
	}
}
