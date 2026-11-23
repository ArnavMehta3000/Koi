#ifndef _KOI_INTEROP_HLSLI_
#define _KOI_INTEROP_HLSLI_

// Shared C++/HLSL structs are written in C++ names
#ifdef __cplusplus
	#include "Koi/Engine/RHI/Types.h"
	#include "Koi/Engine/Math/MathTypes.h"

	#define KOI_INTEROP_BEGIN namespace koi {
	#define KOI_INTEROP_END }
	#define KOI_CHECK_SIZE(type, size) static_assert(sizeof(type) == (size), #type " no longer matches its HLSL layout")
#else
	#define KOI_INTEROP_BEGIN
	#define KOI_INTEROP_END
	#define KOI_CHECK_SIZE(type, size)
	#define alignas(x)

	#define Matrix row_major float4x4
	typedef float4 Vector4;
	typedef float3 Vector3;
	typedef float2 Vector2;
	typedef float  f32;
	typedef uint   u32;
	typedef uint   DescriptorIndex;

	static const DescriptorIndex INVALID_DESCRIPTOR = 0xFFFFFFFFu;
#endif

#endif
