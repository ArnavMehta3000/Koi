#pragma once
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Math/MathTypes.h"
#include <box3d/id.h>
#include <array>
#include <string_view>

namespace koi
{
	class World;

	struct PhysicsSettings
	{
		static constexpr std::string_view kKey = "PhysicsSettings";
		static constexpr std::string_view kCategory = "Physics";

		Vector3 Gravity  = Vector3(0.0f, -9.81f, 0.0f);
		i32     SubSteps = 4;
		bool    DrawColliders = false;  // not in game-release, which has no debug draw

		[[nodiscard]] static const PhysicsSettings& Resolve(const World& world);

		KOI_REFLECT(PhysicsSettings, Gravity, SubSteps, DrawColliders);
	};

	enum class BodyType : u8
	{
		Static,
		Kinematic,
		Dynamic,
	};

	enum class ColliderShape : u8
	{
		Box,
		Sphere,
		Capsule,
	};

	// A Collider without a RigidBody gets a static body
	struct RigidBody
	{
		static constexpr std::string_view kKey = "RigidBody";
		static constexpr std::string_view kCategory = "Physics";

		BodyType Type           = BodyType::Dynamic;
		f32      GravityScale   = 1.0f;
		f32      LinearDamping  = 0.0f;
		f32      AngularDamping = 0.0f;

		std::array<bool, 3> LockPosition{};  // world X, Y, Z
		std::array<bool, 3> LockRotation{};

		Transient<b3BodyId> BodyId = b3_nullBodyId;

		KOI_REFLECT(RigidBody, Type, GravityScale, LinearDamping, AngularDamping, LockPosition, LockRotation, BodyId);
	};

	struct Collider
	{
		static constexpr std::string_view kKey = "Collider";
		static constexpr std::string_view kCategory = "Physics";

		ColliderShape Shape       = ColliderShape::Box;
		Vector3       HalfExtents = Vector3(0.5f, 0.5f, 0.5f);  // Box
		f32           Radius      = 0.5f;                       // Sphere, Capsule
		f32           HalfHeight  = 0.5f;                       // Capsule: cap centers sit at +/- this on local Y
		Vector3       Offset      = Vector3::Zero;

		f32 Friction    = 0.6f;
		f32 Restitution = 0.0f;
		f32 Density     = 1.0f;

		bool IsSensor      = false;  // detects overlaps, never collides
		bool SensorEvents  = true;   // sensors can detect this shape
		bool ContactEvents = false;  // For collision begin/end

		Transient<b3ShapeId> ShapeId    = b3_nullShapeId;
		Transient<Vector3>   BuiltScale = Vector3::Zero;  // the Transform scale ShapeId was built at

		KOI_REFLECT(Collider, Shape, HalfExtents, Radius, HalfHeight, Offset, Friction, Restitution, Density, IsSensor, SensorEvents, ContactEvents, ShapeId, BuiltScale);
	};
}
