#ifndef _MESH_VS_HLSL_
#define _MESH_VS_HLSL_

#include "MeshPushConstants.hlsl"

struct Vertex
{
	float4 Position; // .xyz
	float4 Normal;   // .xyz
	float4 Tangent;
	float4 UV;       // .xy
};

struct VSOutput
{
	float4 Position    : SV_Position;
	float3 WorldNormal : NORMAL;
	float2 UV          : TEXCOORD0;
};

struct FrameConstants
{
	row_major float4x4 ViewProjection;
	float4             CameraPosition;
};

struct PerObjectData
{
	row_major float4x4 World;
	row_major float4x4 NormalMatrix; // transpose(inverse(World)) - only the upper 3x3 is used
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
	StructuredBuffer<Vertex>        vertices = ResourceDescriptorHeap[VertexBufferIndex];
	ConstantBuffer<FrameConstants>  frame    = ResourceDescriptorHeap[FrameConstantsIndex];
	StructuredBuffer<PerObjectData> objects  = ResourceDescriptorHeap[ObjectBufferIndex];

	Vertex v = vertices[vertexId];
	PerObjectData obj = objects[ObjectIndex];

	float4 worldPos = mul(float4(v.Position.xyz, 1.0f), obj.World);

	VSOutput output;
	output.Position    = mul(worldPos, frame.ViewProjection);
	output.WorldNormal = normalize(mul(v.Normal.xyz, (float3x3)obj.NormalMatrix));
	output.UV          = v.UV.xy;

	return output;
}
#endif // _MESH_VS_HLSL_
