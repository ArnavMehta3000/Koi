#ifndef _NORMAL_MAPPING_HLSL_
#define _NORMAL_MAPPING_HLSL_

float3 ApplyNormalMap(
	float3 N,            // Normal
	float4 tangent,      // Tangent
	float2 uv,           // UV
	Texture2D normalTex, // Normal map
	SamplerState samp,   // Sampler
	float scale)         // Scale multipler
{
	if (tangent.w == 0.0f)  // No tangent, no direction to tilt in
		return N;

	float3 normalMap;
	// Remap [0, 1] to [-1, 1]  (BC5 only stores two channels)
	normalMap.xy = normalTex.Sample(samp, uv).rg * 2.0f - 1.0f; 
	// The stored vector has length 1 (so solving for z -> x^2 + y^2 + z^2 = 1)
	// The compression can push it slightly above 1 & sqrt of negative is NaN, saturate clamps it
	normalMap.z = sqrt(saturate(1.0f - dot(normalMap.xy, normalMap.xy)));
	// Apply scale
	normalMap.xy *= scale;

	// Bitangent is perpendicular to both tangent and normal (times sign)
	float3 bitangent = tangent.w * cross(N, tangent.xyz);
	// Combine (same as: mul(n, float3x3(T, B, N)) )
	return normalMap.x * tangent.xyz + normalMap.y * bitangent + normalMap.z * N;
}

#endif // _NORMAL_MAPPING_HLSL_
