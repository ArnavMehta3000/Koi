#include "Interop/DebugDraw.hlsli"

ConstantBuffer<DebugDrawRootConstants> g_root : register(b0);

struct VSOutput
{
	float4 Position : SV_Position;
	float4 Color    : COLOR0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
	StructuredBuffer<DebugVertex> vertices = ResourceDescriptorHeap[g_root.VertexBufferIndex];
	DebugVertex v = vertices[vertexId];

	VSOutput output;
	output.Position = mul(float4(v.Position, 1.0f), g_root.ViewProjection);
	output.Color    = float4(
		 (v.Color        & 0xFF) / 255.0f,
		((v.Color >>  8) & 0xFF) / 255.0f,
		((v.Color >> 16) & 0xFF) / 255.0f,
		((v.Color >> 24) & 0xFF) / 255.0f);

	return output;
}
