cbuffer PushConstants : register(b0)
{
	row_major float4x4 InvViewProjection;
	float4 CameraPosition;   // .xyz used
	uint   AlbedoIndex;
	uint   NormalIndex;
	uint   MaterialIndex;
	uint   DepthIndex;
	uint   LightBufferIndex;
	uint   LightCount;
	uint   ShadowSamplerIndex;
	uint   _Pad;
};

static const uint INVALID_DESCRIPTOR = 0xFFFFFFFFu;

// Must match koi::GPULight (scalar-packed). Type: 0 = directional, 1 = point.
struct GPULight
{
	float3 Position;
	uint   Type;
	float3 Direction;
	float  Range;
	float3 Color;
	float  Intensity;

	row_major float4x4 ShadowMatrix;
	uint               ShadowMapIndex;  // INVALID_DESCRIPTOR when unshadowed
	float              ShadowBias;
	uint2              _Pad;
};

// 1 = fully lit, 0 = fully shadowed
float SampleShadow(GPULight light, float3 worldPos)
{
	if (light.ShadowMapIndex == INVALID_DESCRIPTOR) return 1.0;

	float4 lightClip = mul(float4(worldPos, 1.0), light.ShadowMatrix);
	float3 ndc       = lightClip.xyz / lightClip.w;
	if (any(abs(ndc.xy) > 1.0) || ndc.z > 1.0) return 1.0;  // outside the map

	// Clip space -> UV: y flips because NDC is bottom-up and texture space is top-down
	float2 uv        = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	float  reference = ndc.z - light.ShadowBias;

	Texture2D<float>       shadowMap  = ResourceDescriptorHeap[light.ShadowMapIndex];
	SamplerComparisonState shadowSamp = SamplerDescriptorHeap[ShadowSamplerIndex];

	float2 texelSize;
	shadowMap.GetDimensions(texelSize.x, texelSize.y);
	texelSize = 1.0 / texelSize;

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
	Texture2D<float4> albedoTex   = ResourceDescriptorHeap[AlbedoIndex];
	Texture2D<float4> normalTex   = ResourceDescriptorHeap[NormalIndex];
	Texture2D<float4> materialTex = ResourceDescriptorHeap[MaterialIndex];
	Texture2D<float>  depthTex    = ResourceDescriptorHeap[DepthIndex];

	// The G-buffer is the same size as this pass's target, so SV_Position already names the exact texel
	// Normals and material IDs must not be blended
	// `uv` is still needed below, where the clip-space reconstruction wants it
	int3 texel = int3(pos.xy, 0);

	float depth = depthTex.Load(texel).r;
	if (depth >= 1.0) return float4(0.0, 0.0, 0.0, 1.0);  // background: no geometry here

	// Reconstruct world position from clip space
	float4 clip     = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
	float4 world    = mul(clip, InvViewProjection);
	float3 worldPos = world.xyz / world.w;

	float4 albedoSample = albedoTex.Load(texel);
	float3 albedo       = albedoSample.rgb;
	float  ao           = albedoSample.a;

	// Decode normals to [-1, 1]
	float3 N = normalize(normalTex.Load(texel).xyz * 2.0 - 1.0);

	float  roughness = materialTex.Load(texel).r;  // .g = metallic (unused for now)
	float3 V         = normalize(CameraPosition.xyz - worldPos);

	// TODO: Maybe this is not needed?
	// Small ambient so unlit areas aren't pure black; modulated by baked AO
	float3 color = albedo * 0.03 * ao;

	StructuredBuffer<GPULight> lights = ResourceDescriptorHeap[LightBufferIndex];
	for (uint i = 0; i < LightCount; ++i)
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

		float  ndotl = saturate(dot(N, L));
		float3 H     = normalize(L + V);
		float  ndoth = saturate(dot(N, H));

		// Blinn-Phong specular: map roughness -> shininess
		float shininess = lerp(4.0, 256.0, 1.0 - roughness);
		float spec      = pow(ndoth, shininess) * (1.0 - roughness);

		float3 radiance = light.Color * light.Intensity * atten;
		color += (albedo * ndotl + spec) * radiance * SampleShadow(light, worldPos);
	}

	return float4(color, 1.0);
}
