cbuffer DebugDrawPushConstants : register(b0)
{
	row_major float4x4 ViewProjection;
	uint               VertexBufferIndex;
	uint3              _Pad;
};

// Must match koi::DebugVertex
struct DebugVertex
{
	float3 Position;
	uint   Color;      // RGBA8, low byte is red
};

struct VSOutput
{
	float4 Position : SV_Position;
	float4 Color    : COLOR0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
	StructuredBuffer<DebugVertex> vertices = ResourceDescriptorHeap[VertexBufferIndex];
	DebugVertex v = vertices[vertexId];

	VSOutput output;
	output.Position = mul(float4(v.Position, 1.0f), ViewProjection);
	output.Color    = float4(
		 (v.Color        & 0xFF) / 255.0f,
		((v.Color >>  8) & 0xFF) / 255.0f,
		((v.Color >> 16) & 0xFF) / 255.0f,
		((v.Color >> 24) & 0xFF) / 255.0f);

	return output;
}
