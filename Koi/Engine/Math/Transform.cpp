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
}
