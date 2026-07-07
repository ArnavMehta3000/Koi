struct PSInput
{
	float4 Position    : SV_Position;
	float3 WorldNormal : NORMAL;
	float2 UV          : TEXCOORD0;
};

cbuffer PushConstants : register(b0)
{
	row_major float4x4 MVP;
	uint               VertexBufferIndex;
	uint               BaseColorTextureIndex;
	uint               SamplerIndex;
};

float4 PSMain(PSInput input) : SV_Target
{
	Texture2D    baseColorTex = ResourceDescriptorHeap[BaseColorTextureIndex];
	SamplerState samp         = SamplerDescriptorHeap[SamplerIndex];

	float3 albedo = baseColorTex.Sample(samp, input.UV).rgb;

	float3 N = normalize(input.WorldNormal);
	float3 L = normalize(float3(0.4f, 0.8f, 0.3f));

	float  ndotl   = saturate(dot(N, L));
	float3 ambient = 0.15f * albedo;
	float3 color   = ambient + albedo * ndotl;

	return float4(color, 1.0f);
}
