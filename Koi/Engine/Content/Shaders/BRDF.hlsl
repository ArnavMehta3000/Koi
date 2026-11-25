#ifndef _BRDF_HLSL_
#define _BRDF_HLSL_

// Ref: https://graphicscompendium.com/theory/08-cook-torrance-ggx

static const float kMinRoughness  = 0.089;                    // Keeps the GGX peak in half-float range
static const float kPi            = 3.14159265;
static const float3 kDielectricF0 = float3(0.04, 0.04, 0.04); // IOR: 1.5

// GGX: density of microfacets facing H
float D_GGX(float ndoth, float a)
{
	float a2 = a * a;
	float d = ndoth * ndoth * (a2 - 1.0) + 1.0;
	return a2 / (kPi * d * d);
}

// Schlick's Fresnel: fraction of light reflected
float3 F_Schlick(float vdoth, float3 F0)
{
	float m = saturate(1.0 - vdoth);
	float m2 = m * m;
	return F0 + (1.0 - F0) * (m2 * m2 * m);
}

#endif // _BRDF_HLSL_
