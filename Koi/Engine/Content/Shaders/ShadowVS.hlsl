#ifndef _SHADOW_VS_HLSL_
#define _SHADOW_VS_HLSL_

#include "MeshShaderCommon.hlsl"

struct Vertex
{
	float3 Position;
	float3 Normal;
	float4 Tangent;
	float2 UV;
};

struct FrameConstants
{
	row_major float4x4 ViewProjection;  // light-space, not camera-space
	float4             CameraPosition;  // unused here
};

struct PerObjectData
{
	row_major float4x4 World;
	row_major float4x4 NormalMatrix;    // unused here
};

// Depth-only pass: position is the whole output, so there is no pixel shader at all
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
	StructuredBuffer<Vertex>        vertices = ResourceDescriptorHeap[VertexBufferIndex];
	ConstantBuffer<FrameConstants>  frame    = ResourceDescriptorHeap[FrameConstantsIndex];
	StructuredBuffer<PerObjectData> objects  = ResourceDescriptorHeap[ObjectBufferIndex];

	Vertex v        = vertices[vertexId + VertexOffset];
	float4 worldPos = mul(float4(v.Position, 1.0f), objects[ObjectIndex].World);

	return mul(worldPos, frame.ViewProjection);
}

#endif // _SHADOW_VS_HLSL_
