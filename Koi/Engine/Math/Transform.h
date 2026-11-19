#pragma once
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Math/MathTypes.h"

namespace koi
{
	struct Transform
	{
		static constexpr std::string_view kKey = "Transform";

		Vector3 Position    = Vector3::Zero;
		Quaternion Rotation = Quaternion::Identity;
		Vector3 Scale       = Vector3::One;

		[[nodiscard]] Matrix ToMatrix() const;

		KOI_REFLECT(Transform, Position, Rotation, Scale);
	};

	// Rebuilt every frame, never-saved
	struct WorldTransform
	{
		Matrix     World    = Matrix::Identity;
		Vector3    Position = Vector3::Zero;
		Quaternion Rotation = Quaternion::Identity;
		Vector3    Scale    = Vector3::One;  // lossy under rotated non-uniform parents

		[[nodiscard]] static WorldTransform Compose(const Transform& local, const WorldTransform* parent);

		// The local Transform that places an entity at `world` under `parent` (null when top level)
		[[nodiscard]] static Transform ToLocal(Matrix world, const WorldTransform* parent);

		[[nodiscard]] Transform AsTransform() const { return { Position, Rotation, Scale }; }
	};
}
