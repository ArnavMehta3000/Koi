#ifndef _BRDF_HLSL_
#define _BRDF_HLSL_

// Ref: https://graphicscompendium.com/theory/08-cook-torrance-ggx

static const float kPi = 3.14159265;
static const float kMinRoughness = 0.089; // Keeps the GGX peak in half-float range

// GGX: density of microfacets facing H
float D_GGX(float ndoth, float a)
{
	float a2 = a * a;
	float d = ndoth * ndoth * (a2 - 1.0) + 1.0;
	return a2 / (kPi * d * d);
}

#endif // _BRDF_HLSL_
