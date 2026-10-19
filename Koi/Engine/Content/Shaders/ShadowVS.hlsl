#ifndef _SHADOW_VS_HLSL_
#define _SHADOW_VS_HLSL_

#include "MeshShaderCommon.hlsl"

// Depth-only pass: position is the whole output, so there is no pixel shader at all
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
	StructuredBuffer<Vertex>        vertices = ResourceDescriptorHeap[g_root.VertexBufferIndex];
	ConstantBuffer<FrameConstants>  frame    = ResourceDescriptorHeap[g_root.FrameConstantsIndex];
	StructuredBuffer<PerObjectData> objects  = ResourceDescriptorHeap[g_root.ObjectBufferIndex];

	Vertex v        = vertices[vertexId + g_root.VertexOffset];
	float4 worldPos = mul(float4(v.Position, 1.0f), objects[g_root.ObjectIndex].World);

	return mul(worldPos, frame.ViewProjection);
}

#endif // _SHADOW_VS_HLSL_
