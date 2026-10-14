#include "Tonemapping.hlsl"

cbuffer PushConstants : register(b0)
{
	uint HDRTextureIndex;
};

float4 PSMain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	Texture2D<float4> hdrTex = ResourceDescriptorHeap[HDRTextureIndex];

	float3 hdr = hdrTex.Load(int3(pos.xy, 0)).rgb;

	float3 ldr = ACES(hdr);
	return float4(ldr, 1.0);
}
