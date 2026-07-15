cbuffer PushConstants : register(b0)
{
	uint HDRTextureIndex;
};

// Narkowicz ACES approximation.
// Maps HDR luminance [0, inf) -> [0, 1] with a filmic S-curve.
float3 ACES(float3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	Texture2D<float4> hdrTex = ResourceDescriptorHeap[HDRTextureIndex];

	float3 hdr = hdrTex.Load(int3(pos.xy, 0)).rgb;

	float3 ldr = ACES(hdr);
	return float4(ldr, 1.0);
}
