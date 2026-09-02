#ifndef _MESH_PS_HLSL_
#define _MESH_PS_HLSL_

#include "MeshShaderCommon.hlsl"

struct PSInput
{
	float4 Position    : SV_Position;
	float3 WorldNormal : NORMAL;
	float2 UV          : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
	ConstantBuffer<FrameConstants>    frame     = ResourceDescriptorHeap[FrameConstantsIndex];
	StructuredBuffer<GPUMaterialData> materials = ResourceDescriptorHeap[frame.MaterialBufferIndex];
	GPUMaterialData mat = materials[MaterialIndex];

	Texture2D    baseColorTex = ResourceDescriptorHeap[mat.BaseColorIndex];
	SamplerState samp         = SamplerDescriptorHeap[SamplerIndex];

	float3 albedo = baseColorTex.Sample(samp, input.UV).rgb * mat.BaseColorFactor.rgb;

	float3 N = normalize(input.WorldNormal);
	float3 L = normalize(float3(0.4f, 0.8f, 0.3f));

	float  ndotl   = saturate(dot(N, L));
	float3 ambient = 0.15f * albedo;
	float3 color   = ambient + albedo * ndotl;

	return float4(color, 1.0f);
}
#endif // _MESH_PS_HLSL_
