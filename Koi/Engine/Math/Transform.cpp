#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	Matrix Transform::ToMatrix() const
	{
		const Matrix S = Matrix::CreateScale(Scale);
		const Matrix R = Matrix::CreateFromQuaternion(Rotation);
		const Matrix T = Matrix::CreateTranslation(Position);
		return S * R * T;
	}

	WorldTransform WorldTransform::Compose(const Transform& local, const WorldTransform* parent)
	{
		if (!parent) return { local.ToMatrix(), local.Position, math::SafeNormalize(local.Rotation), local.Scale };

		const Matrix ogWorld = local.ToMatrix() * parent->World;

		WorldTransform world
		{
			.World    = ogWorld,
			.Position = ogWorld.Translation(),
			.Rotation = math::SafeNormalize(local.Rotation * parent->Rotation),
			.Scale    = local.Scale * parent->Scale,
		};
		return world;
	}

	Transform WorldTransform::ToLocal(Matrix world, const WorldTransform* parent)
	{
		Matrix local = world;
		if (parent)
		{
			// A zero-scale parent can't be inverted, so only undo its rotation and position
			const Matrix parentMatrix = math::IsSingular(parent->World)
				? Matrix::CreateFromQuaternion(parent->Rotation) * Matrix::CreateTranslation(parent->Position)
				: parent->World;
			local = world * parentMatrix.Invert();
		}

		Transform result;
		if (!local.Decompose(result.Scale, result.Rotation, result.Position))
		{
			result = Transform{ .Position = local.Translation() };
		}
		result.Rotation = math::SafeNormalize(result.Rotation);
		return result;
	}
}
