#ifndef KOI_TONEMAPPING_HLSL
#define KOI_TONEMAPPING_HLSL

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

#endif
