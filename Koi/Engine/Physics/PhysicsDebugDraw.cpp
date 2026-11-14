#include "Koi/Engine/Physics/PhysicsDebugDraw.h"
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Physics/Box3DConvert.h"
#include "Koi/Engine/Physics/ScaledCollider.h"
#include <box3d/box3d.h>

namespace koi
{
	namespace
	{
		Color ColorFor(const Collider& collider, b3BodyType type)
		{
			if (collider.IsSensor)          return Color(1.0f, 0.85f, 0.2f, 1.0f);
			if (type == b3_kinematicBody)   return Color(0.3f, 0.6f, 1.0f, 1.0f);
			if (type == b3_dynamicBody)     return Color(0.4f, 1.0f, 0.4f, 1.0f);
			return Color(0.6f, 0.6f, 0.6f, 1.0f);
		}
	}

	void PhysicsDebugDrawSystem::Run(const SystemContext& ctx)
	{
		if (!PhysicsSettings::Resolve(ctx.World).DrawColliders) return;

		ctx.World.GetECS().Each<const WorldTransform, const Collider>([](const WorldTransform& xform, const Collider& collider)
		{
			const b3ShapeId shapeId = collider.ShapeId.get();
			if (!::b3Shape_IsValid(shapeId)) return;

			const b3BodyId body = ::b3Shape_GetBody(shapeId);
			const b3WorldTransform pose = ::b3Body_GetTransform(body);
			const Vector3 position    = internal::ToKoi(pose.p);
			const Quaternion rotation = internal::ToKoi(pose.q);

			const internal::ScaledCollider shape = internal::ScaleCollider(collider, xform.Scale);
			const Vector3 center = position + Vector3::Transform(shape.Center, rotation);
			const Color color    = ColorFor(collider, ::b3Body_GetType(body));

			switch (collider.Shape)
			{
			case ColliderShape::Box:
				DebugDraw::Box(BoundingOrientedBox(center, shape.HalfExtents, rotation), color);
				break;
			case ColliderShape::Sphere:
				DebugDraw::Sphere(center, shape.Radius, color);
				break;
			case ColliderShape::Capsule:
			{
				const Vector3 axis = Vector3::Transform(Vector3(0.0f, shape.HalfHeight, 0.0f), rotation);
				DebugDraw::Capsule(center - axis, center + axis, shape.Radius, color);
				break;
			}
			}
		});
	}
}
