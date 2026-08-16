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
	uint2  _Pad;
};

// Must match koi::GPULight (scalar-packed). Type: 0 = directional, 1 = point.
struct GPULight
{
	float3 Position;
	uint   Type;
	float3 Direction;
	float  Range;
	float3 Color;
	float  Intensity;
};

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
		color += (albedo * ndotl + spec) * radiance;
	}

	return float4(color, 1.0);
}
