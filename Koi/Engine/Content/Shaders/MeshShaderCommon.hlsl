#ifndef _MESH_SHADER_COMMON_HLSL_
#define _MESH_SHADER_COMMON_HLSL_

cbuffer DrawPushConstants : register(b0)
{
	uint FrameConstantsIndex;
	uint ObjectBufferIndex;
	uint ObjectIndex;
	uint MaterialIndex;      // slot in the one material table, which FrameConstants names
	uint VertexBufferIndex;
	uint SamplerIndex;
	uint VertexOffset;
};

static const uint INVALID_DESCRIPTOR = 0xFFFFFFFFu;

// Must match koi::FrameConstants (Koi/Engine/Graphics/GPUSceneTypes.h)
struct FrameConstants
{
	row_major float4x4 ViewProjection;
	float4             CameraPosition;
	uint               MaterialBufferIndex;
	uint3              _Pad;
};

struct GPUMaterialData
{
	float4 BaseColorFactor;
	float4 EmissiveFactor;
	float  MetallicFactor;
	float  RoughnessFactor;
	float  NormalScale;
	float  OcclusionStrength;
	float  AlphaCutoff;
	uint   BaseColorIndex;
	uint   MetallicRoughnessIndex;
	uint   NormalIndex;
	uint   OcclusionIndex;
	uint   EmissiveIndex;
	uint2  _Pad;
};

#endif // _MESH_SHADER_COMMON_HLSL_
