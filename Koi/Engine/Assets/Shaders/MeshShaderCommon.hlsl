#ifndef _MESH_SHADER_COMMON_HLSL_
#define _MESH_SHADER_COMMON_HLSL_

cbuffer PushConstants : register(b0)
{
	uint FrameConstantsIndex;
	uint ObjectBufferIndex;
	uint ObjectIndex;
	uint MaterialBufferIndex;
	uint MaterialIndex;
	uint VertexBufferIndex;
	uint SamplerIndex;
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
