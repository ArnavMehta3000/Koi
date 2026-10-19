#ifndef _GBUFFER_PS_HLSL_
#define _GBUFFER_PS_HLSL_

#include "MeshShaderCommon.hlsl"

struct PSInput
{
	float4 Position    : SV_Position;
	float3 WorldNormal : NORMAL;  // [-1, 1] range
	float2 UV          : TEXCOORD0;
};

struct PSOutput
{
	float4 Color    : SV_Target0;  // Albedo.rgb + AO
	float4 Normal   : SV_Target1;  // Normals [0, 1] range
	float4 Material : SV_Target2;  // Roughness/Metallic/Specular
};

PSOutput PSMain(PSInput input)
{
	ConstantBuffer<FrameConstants>    frame     = ResourceDescriptorHeap[g_root.FrameConstantsIndex];
	StructuredBuffer<GPUMaterialData> materials = ResourceDescriptorHeap[frame.MaterialBufferIndex];
	GPUMaterialData mat = materials[g_root.MaterialIndex];

	Texture2D baseColorTex = ResourceDescriptorHeap[mat.BaseColorIndex];
	SamplerState samp      = SamplerDescriptorHeap[g_root.SamplerIndex];

	// Albedo: base color texture (always valid, CPU falls back to white) * factor
	const float3 albedo = baseColorTex.Sample(samp, input.UV).rgb * mat.BaseColorFactor.rgb;

	// Metallic-roughness: glTF packs roughness in G, metallic in B of one texture
	float roughness = mat.RoughnessFactor;
	float metallic  = mat.MetallicFactor;
	if (mat.MetallicRoughnessIndex != INVALID_DESCRIPTOR)
	{
		Texture2D mrTex = ResourceDescriptorHeap[mat.MetallicRoughnessIndex];
		float2 mr       = mrTex.Sample(samp, input.UV).gb;  // g = roughness, b = metallic

		roughness *= mr.x;
		metallic  *= mr.y;
	}

	// Ambient occlusion: glTF stores it in the R channel of an optional texture,
	// scaled toward 1.0 by OcclusionStrength (spec: 1 + strength * (sampled - 1))
	float ao = 1.0f;
	if (mat.OcclusionIndex != INVALID_DESCRIPTOR)
	{
		Texture2D aoTex = ResourceDescriptorHeap[mat.OcclusionIndex];
		float sampledAO = aoTex.Sample(samp, input.UV).r;
		ao = 1.0f + mat.OcclusionStrength * (sampledAO - 1.0f);
	}

	// Encode world normal from [-1, 1] to [0, 1] for the UNORM target
	const float3 mappedNormal = input.WorldNormal * 0.5f + 0.5f;

	PSOutput output;
	output.Color    = float4(albedo, ao);
	output.Normal   = float4(mappedNormal, 1.0f);
	output.Material = float4(roughness, metallic, 0.0f, 1.0f);

	return output;
}

#endif // _GBUFFER_PS_HLSL_
