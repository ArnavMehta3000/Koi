#include "MeshShaderCommon.hlsl"

struct VSOutput
{
	float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
	StructuredBuffer<Vertex>        vertices = ResourceDescriptorHeap[g_root.VertexBufferIndex];
	ConstantBuffer<FrameConstants>  frame    = ResourceDescriptorHeap[g_root.FrameConstantsIndex];
	StructuredBuffer<PerObjectData> objects  = ResourceDescriptorHeap[g_root.ObjectBufferIndex];

	Vertex v        = vertices[vertexId + g_root.VertexOffset];
	float4 worldPos = mul(float4(v.Position, 1.0f), objects[g_root.ObjectIndex].World);

	VSOutput output;
	output.Position = mul(worldPos, frame.ViewProjection);
	output.UV       = v.UV;

	return output;
}
