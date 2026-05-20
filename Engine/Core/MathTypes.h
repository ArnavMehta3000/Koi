// Lightweight hlslpp include
#pragma once

#include "Core/Config.h"

#if KOI_DEBUG
	#define HLSLPP_ASSERT
#endif

#include <hlsl++/matrix_float_type.h>
#include <hlsl++/quaternion_type.h>
#include <hlsl++/vector_double_type.h>
#include <hlsl++/vector_float_type.h>
#include <hlsl++/vector_int_type.h>
#include <hlsl++/vector_uint_type.h>

namespace koi::math
{
	// Pull hlslpp types into Koi's math namespace
	using namespace hlslpp;

	struct AABB;
}
