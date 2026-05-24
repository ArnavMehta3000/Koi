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

// Root 32-bit constants (b0). hlslpp stores matrices row-major.
cbuffer PushConstants : register(b0)
{
	row_major float4x4 MVP;
	uint               VertexBufferIndex;
	uint               BaseColorTextureIndex;
	uint               SamplerIndex;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
	StructuredBuffer<Vertex> vertices = ResourceDescriptorHeap[VertexBufferIndex];
	Vertex v = vertices[vertexId];

	VSOutput output;
	output.Position    = mul(float4(v.Position.xyz, 1.0f), MVP);
	output.WorldNormal = normalize(v.Normal.xyz);
	output.UV          = v.UV.xy;

	return output;
}
