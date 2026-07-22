#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	// CBV, one per frame-in-flight
	struct  FrameConstants
	{
		Matrix ViewProjection;
		Vector4 CameraPosition; // .xyz used, .w unused
	};

	// StructuredBuffer element, one per drawn entity. Rebuilt every frame
	struct alignas(16) PerObjectData
	{
		Matrix World;
		Matrix NormalMatrix; // transpose(inverse(World)); shader uses the upper 3x3
	};

	// StructuredBuffer element, one per material
	struct alignas(16) GPUMaterialData
	{
		Vector4 BaseColorFactor;
		Vector4 EmissiveFactor; // .xyz used, .w unused
		f32     MetallicFactor;
		f32     RoughnessFactor;
		f32     NormalScale;
		f32     OcclusionStrength;
		f32     AlphaCutoff;
		u32     BaseColorIndex;
		u32     MetallicRoughnessIndex;
		u32     NormalIndex;
		u32     OcclusionIndex;
		u32     EmissiveIndex;
		u32     _Pad[2];
	};

#pragma warning(disable : 4324) // structure was padded due to alignment specifier
	struct alignas(16) DrawPushConstants
	{
		u32 FrameConstantsIndex;
		u32 ObjectBufferIndex;
		u32 ObjectIndex;
		u32 MaterialBufferIndex;
		u32 MaterialIndex;
		u32 VertexBufferIndex;
		u32 SamplerIndex;
	};
#pragma warning(default : 4324)
}
