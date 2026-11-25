#include "Interop/Lighting.hlsli"
#include "BRDF.hlsl"

ConstantBuffer<LightingRootConstants> g_root : register(b0);

// 1 = fully lit, 0 = fully shadowed
float SampleShadow(GPULight light, float3 worldPos, float3 N, float ndotl)
{
	if (light.ShadowMapIndex == INVALID_DESCRIPTOR) return 1.0;

	Texture2D<float>       shadowMap  = ResourceDescriptorHeap[light.ShadowMapIndex];
	SamplerComparisonState shadowSamp = SamplerDescriptorHeap[g_root.ShadowSamplerIndex];

	float2 texelSize;
	shadowMap.GetDimensions(texelSize.x, texelSize.y);
	texelSize = 1.0 / texelSize;

	// Normal offset: push the lookup off the surface by about one shadow texel in world units
	float  worldPerTexel = 2.0 / max(length(light.ShadowMatrix._11_21_31), 1e-6) * texelSize.x;
	float  slope         = sqrt(saturate(1.0 - ndotl * ndotl));
	float3 samplePos     = worldPos + N * worldPerTexel * (1.0 + 2.0 * slope);

	float4 lightClip = mul(float4(samplePos, 1.0), light.ShadowMatrix);
	float3 ndc       = lightClip.xyz / lightClip.w;
	if (any(abs(ndc.xy) > 1.0) || ndc.z > 1.0) return 1.0;  // outside the map

	// Clip space -> UV: y flips because NDC is bottom-up and texture space is top-down
	float2 uv        = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	float  reference = ndc.z - light.ShadowBias;

	// 3x3 PCF. Each tap is already bilinear-filtered across 4 texels by the comparison
	// sampler, so this is a 4x4-ish kernel for 9 taps
	float sum = 0.0;
	[unroll]
	for (int y = -1; y <= 1; ++y)
	{
		[unroll]
		for (int x = -1; x <= 1; ++x)
		{
			sum += shadowMap.SampleCmpLevelZero(shadowSamp, uv + float2(x, y) * texelSize, reference);
		}
	}

	return sum / 9.0;
}

float4 PSMain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	Texture2D<float4> albedoTex   = ResourceDescriptorHeap[g_root.AlbedoIndex];
	Texture2D<float4> normalTex   = ResourceDescriptorHeap[g_root.NormalIndex];
	Texture2D<float4> materialTex = ResourceDescriptorHeap[g_root.MaterialIndex];
	Texture2D<float>  depthTex    = ResourceDescriptorHeap[g_root.DepthIndex];

	// The G-buffer is the same size as this pass's target, so SV_Position already names the exact texel
	// Normals and material IDs must not be blended
	// `uv` is still needed below, where the clip-space reconstruction wants it
	int3 texel = int3(pos.xy, 0);

	float depth = depthTex.Load(texel).r;
	if (depth >= 1.0) return float4(0.0, 0.0, 0.0, 1.0);  // background: no geometry here

	// Reconstruct world position from clip space
	float4 clip     = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
	float4 world    = mul(clip, g_root.InvViewProjection);
	float3 worldPos = world.xyz / world.w;

	float4 albedoSample = albedoTex.Load(texel);
	float3 albedo       = albedoSample.rgb;
	float  ao           = albedoSample.a;

	// Decode normals to [-1, 1]
	float3 N = normalize(normalTex.Load(texel).xyz * 2.0 - 1.0);

	float2 material  = materialTex.Load(texel).rg;
	float roughness  = max(material.r, kMinRoughness);
	float a          = roughness * roughness;
	float metallic   = material.g;
	float3 V         = normalize(g_root.CameraPosition.xyz - worldPos);

	float3 F0            = lerp(kDielectricF0, albedo, metallic);
	float3 diffuseAlbedo = albedo * (1.0 - metallic);

	// TODO: Maybe this is not needed?
	// Small ambient so unlit areas aren't pure black; modulated by baked AO
	float3 color = albedo * 0.03 * ao;

	StructuredBuffer<GPULight> lights = ResourceDescriptorHeap[g_root.LightBufferIndex];
	for (uint i = 0; i < g_root.LightCount; ++i)
	{
		GPULight light = lights[i];

		float3 L     = float3(0.0, 0.0, 0.0);
		float  atten = 1.0;
		if (light.Type == 0)  // directional: points along the entity's forward
		{
			L = normalize(-light.Direction);
		}
		else                  // point: attenuate toward Range
		{
			float3 toLight = light.Position - worldPos;
			float  dist    = length(toLight);
			L              = toLight / max(dist, 1e-4);
			float d        = saturate(1.0 - dist / max(light.Range, 1e-4));
			atten          = d * d;
		}

		float ndotl = saturate(dot(N, L));
		float3 H    = normalize(L + V);
		float ndoth = saturate(dot(N, H));
		float vdoth = saturate(dot(V, H));

		float3 F        = F_Schlick(vdoth, F0);
		float3 diffuse  = (1.0 - F) * diffuseAlbedo / kPi;
		float3 specular = F * D_GGX(ndoth, a) * 0.25; // G cancels the denominator for now

		float3 radiance = light.Color * light.Intensity * atten;
		color += (diffuse + specular) * ndotl * radiance * SampleShadow(light, worldPos, N, ndotl);
	}

	return float4(color, 1.0);
}
