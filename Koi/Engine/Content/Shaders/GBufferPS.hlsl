#ifndef _GBUFFER_PS_HLSL_
#define _GBUFFER_PS_HLSL_

#include "MeshShaderCommon.hlsl"
#include "NormalMapping.hlsl"

struct PSInput
{
	float4 Position     : SV_Position;
	float3 WorldNormal  : NORMAL;  // [-1, 1] range
	float4 WorldTangent : TANGENT;
	float2 UV           : TEXCOORD0;
	bool   FrontFace    : SV_IsFrontFace;
};

struct PSOutput
{
	float4 Color    : SV_Target0;  // Albedo.rgb + AO
	float4 Normal   : SV_Target1;  // Normals [0, 1] range
	float4 Material : SV_Target2;  // Roughness/Metallic/Specular
	float4 Emissive : SV_Target3;  // HDR scene color, lighting adds to it
};

PSOutput PSMain(PSInput input)
{
	ConstantBuffer<FrameConstants>    frame     = ResourceDescriptorHeap[g_root.FrameConstantsIndex];
	StructuredBuffer<GPUMaterialData> materials = ResourceDescriptorHeap[frame.MaterialBufferIndex];
	GPUMaterialData mat = materials[g_root.MaterialIndex];

	Texture2D baseColorTex = ResourceDescriptorHeap[mat.BaseColorIndex];
	SamplerState samp      = SamplerDescriptorHeap[g_root.SamplerIndex];

	// Albedo: base color texture (always valid, CPU falls back to white) * factor
	const float4 baseColor = baseColorTex.Sample(samp, input.UV) * mat.BaseColorFactor;
	const float3 albedo    = baseColor.rgb;

#ifdef ALPHA_TEST
	clip(baseColor.a - mat.AlphaCutoff);  // Discard current pixel if val < 0
#endif

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

	float3 emissive = mat.EmissiveFactor.rgb;
	if (mat.EmissiveIndex != INVALID_DESCRIPTOR)
	{
		Texture2D emissiveTex = ResourceDescriptorHeap[mat.EmissiveIndex];
		emissive *= emissiveTex.Sample(samp, input.UV).rgb;
	}

	// MikkTSpace wants the interpolated normal as it is, so only the result gets normalized
	float3 N = input.WorldNormal;
	if (mat.NormalIndex != INVALID_DESCRIPTOR)
	{
		Texture2D normalTex = ResourceDescriptorHeap[mat.NormalIndex];
		N = ApplyNormalMap(N, input.WorldTangent, input.UV, normalTex, samp, mat.NormalScale);
	}

	N = normalize(N) * (input.FrontFace ? 1.0f : -1.0f);

	// Encode world normal from [-1, 1] to [0, 1] for the UNORM target
	const float3 mappedNormal = N * 0.5f + 0.5f;

	PSOutput output;
	output.Color    = float4(albedo, ao);
	output.Normal   = float4(mappedNormal, 1.0f);
	output.Material = float4(roughness, metallic, 0.0f, 1.0f);
	output.Emissive = float4(emissive, 1.0f);

	return output;
}

#endif // _GBUFFER_PS_HLSL_
