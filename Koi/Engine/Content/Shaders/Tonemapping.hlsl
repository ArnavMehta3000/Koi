#ifndef KOI_TONEMAPPING_HLSL
#define KOI_TONEMAPPING_HLSL
#include "Interop/PostProcess.hlsli"

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

// Benjamin Wrensch's AgX fit
float3 AgX(float3 color)
{
	const float3x3 inset = float3x3(
		0.842479062253094,  0.0423282422610123, 0.0423756549057051,
		0.0784335999999992, 0.878468636469772,  0.0784336,
		0.0792237451477643, 0.0791661274605434, 0.879142973793104);
	const float3x3 outset = float3x3(
		 1.19687900512017,   -0.0528968517574562, -0.0529716355144438,
		-0.0980208811401368,  1.15190312990417,   -0.0980434501171241,
		-0.0990297440797205, -0.0989611768448433,  1.15107367264116);
	const float minEV = -12.47393;
	const float maxEV = 4.026069;

	float3 x = mul(max(color, 1e-10), inset);
	x = (clamp(log2(x), minEV, maxEV) - minEV) / (maxEV - minEV);

	const float3 x2 = x * x;
	const float3 x4 = x2 * x2;
	x = 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;

	return pow(saturate(mul(x, outset)), 2.2);
}

// Khronos PBR Neutral
float3 PBRNeutral(float3 color)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	const float x = min(color.r, min(color.g, color.b));
	color -= x < 0.08 ? x - 6.25 * x * x : 0.04;

	const float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression)
		return color;

	const float d = 1.0 - startCompression;
	const float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	const float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return lerp(color, newPeak.xxx, g);
}

// John Hable's Uncharted 2 filmic curve, white point 11.2
float3 HableCurve(float3 x)
{
	const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 Hable(float3 color)
{
	return saturate(HableCurve(color * 2.0) / HableCurve(11.2.xxx));
}

// On luminance, so saturated highlights keep their hue
float3 Reinhard(float3 color)
{
	const float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
	return saturate(color / (1.0 + lum));
}

float3 Tonemap(float3 color, uint op)
{
	switch (op)
	{
		case TONEMAPPER_AGX:         return AgX(color);
		case TONEMAPPER_PBR_NEUTRAL: return saturate(PBRNeutral(color));
		case TONEMAPPER_HABLE:       return Hable(color);
		case TONEMAPPER_REINHARD:    return Reinhard(color);
		case TONEMAPPER_NONE:        return saturate(color);
		default:                     return ACES(color);
	}
}

// The exact sRGB transfer curve. Display targets are UNORM, so the encode happens here
float3 LinearToSRGB(float3 c)
{
	return select(c <= 0.0031308, c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055);
}

#endif
