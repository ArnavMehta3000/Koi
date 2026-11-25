#include "Interop/Sky.hlsli"

static const float kPi = 3.14159265;

ConstantBuffer<SkyRootConstants> g_root : register(b0);

// Center of the image is -Z, its top row is +Y
float2 DirectionToLatLong(float3 dir, float rotation)
{
	float longitude = atan2(dir.x, -dir.z) - rotation;
	float latitude = acos(clamp(dir.y, -1.0, 1.0));
	return float2(longitude / (2.0 * kPi) + 0.5, latitude / kPi);
}

float4 PSMain(float4 pos : SV_Position, float2 ndc : TEXCOORD0) : SV_Target
{
	// Get world view direction for this pixel
	float4 farPoint = mul(float4(ndc, 1.0, 1.0), g_root.InvViewProjection);
	float3 dir      = normalize(farPoint.xyz / farPoint.w - g_root.CameraPosition);

	Texture2D<float4> image = ResourceDescriptorHeap[g_root.ImageIndex];
	SamplerState samp       = SamplerDescriptorHeap[g_root.SamplerIndex];

	// Convert looking direction to UV for sampling
	float2 uv       = DirectionToLatLong(dir, g_root.Rotation);
	float3 radiance = image.SampleLevel(samp, uv, 0).rgb * g_root.Radiance;
	return float4(radiance, 1.0);
}
