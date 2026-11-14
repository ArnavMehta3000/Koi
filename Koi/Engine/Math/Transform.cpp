#include "Koi/Engine/Math/Transform.h"

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
		if (!parent) return { local.ToMatrix(), local.Position, local.Rotation, local.Scale };

		const Matrix ogWorld = local.ToMatrix() * parent->World;

		WorldTransform world
		{
			.World    = ogWorld,
			.Position = ogWorld.Translation(),
			.Rotation = local.Rotation * parent->Rotation,
			.Scale    = local.Scale * parent->Scale,
		};

		world.Rotation.Normalize();
		return world;
	}

	Transform WorldTransform::ToLocal(Matrix world, const WorldTransform* parent)
	{
		Matrix local = parent ? world * parent->World.Invert() : world;

		Transform result;
		if (!local.Decompose(result.Scale, result.Rotation, result.Position))
		{
			result.Position = local.Translation();
		}
		return result;
	}
}
