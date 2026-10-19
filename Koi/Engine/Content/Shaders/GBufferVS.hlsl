#ifndef _GBUFFER_VS_HLSL_
#define _GBUFFER_VS_HLSL_

#include "MeshShaderCommon.hlsl"

struct VSOutput
{
	float4 Position    : SV_Position;
	float3 WorldNormal : NORMAL;
	float2 UV          : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
	StructuredBuffer<Vertex>        vertices = ResourceDescriptorHeap[g_root.VertexBufferIndex];
	ConstantBuffer<FrameConstants>  frame    = ResourceDescriptorHeap[g_root.FrameConstantsIndex];
	StructuredBuffer<PerObjectData> objects  = ResourceDescriptorHeap[g_root.ObjectBufferIndex];

	Vertex v = vertices[vertexId + g_root.VertexOffset];
	PerObjectData obj = objects[g_root.ObjectIndex];

	float4 worldPos = mul(float4(v.Position, 1.0f), obj.World);

	VSOutput output;
	output.Position    = mul(worldPos, frame.ViewProjection);
	output.WorldNormal = normalize(mul(v.Normal, (float3x3)obj.NormalMatrix));
	output.UV          = v.UV;

	return output;
}

#endif // _GBUFFER_VS_HLSL_
